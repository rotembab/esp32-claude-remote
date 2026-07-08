"""
Claude Remote — bridge server.

Runs on your PC/Mac. Drives a real, multi-turn Claude Code session using the
Claude Agent SDK and exposes it over a WebSocket that the ESP32 handheld (or the
browser test client) connects to.

Wire protocol (JSON text frames + raw binary audio frames):

  client -> bridge
    {"type":"prompt", "text": "..."}          send a user turn
    {"type":"interrupt"}                        stop the current response
    {"type":"new_session"}                      reset the session
    {"type":"permission", "id":"..","allow":b}  approve/deny a tool-use request
    {"type":"set_mode", "mode":"default|acceptEdits|plan"}
    {"type":"cycle_mode"}                        advance to the next mode (Shift+Tab)
    {"type":"audio_start"}                       begin a push-to-talk recording
    <binary frames>                              raw 16-bit LE PCM mono @16kHz
    {"type":"audio_end"}                         end recording -> transcribe

  bridge -> client
    {"type":"status", "state":"..","mode":".."}  state: ready|thinking|running_tool|done
    {"type":"text", "delta":"..."}               streamed assistant text
    {"type":"tool_use", "name":"..","input":{}}  a tool is being invoked
    {"type":"permission_request","id":"..","tool":"..","input":{}}
    {"type":"result", "text":"..."}              turn finished
    {"type":"transcript", "text":"..."}          speech-to-text result
    {"type":"error", "message":"..."}

Run:  python bridge.py
"""

import asyncio
import json
import os
import uuid

import numpy as np
import websockets
from dotenv import load_dotenv

from claude_agent_sdk import (
    ClaudeSDKClient,
    ClaudeAgentOptions,
    AssistantMessage,
    ResultMessage,
    TextBlock,
    ToolUseBlock,
    PermissionResultAllow,
    PermissionResultDeny,
)

load_dotenv()

HOST = os.getenv("BRIDGE_HOST", "0.0.0.0")
PORT = int(os.getenv("BRIDGE_PORT", "8765"))
SESSION_CWD = os.getenv("SESSION_CWD", ".")
WHISPER_MODEL = os.getenv("WHISPER_MODEL", "base")

# Permission-mode cycle order, matching Claude Code's Shift+Tab.
MODES = ["default", "acceptEdits", "plan"]

# Tool classification used to implement the modes bridge-side.
READONLY_TOOLS = {
    "Read", "Glob", "Grep", "LS", "NotebookRead", "WebFetch", "WebSearch", "TodoWrite",
}
EDIT_TOOLS = {"Edit", "Write", "MultiEdit", "NotebookEdit"}

# ---- Speech-to-text (lazy loaded so the server starts instantly) -----------
_whisper = None


def get_whisper():
    global _whisper
    if _whisper is None:
        from faster_whisper import WhisperModel
        print(f"[stt] loading whisper model '{WHISPER_MODEL}' ...")
        _whisper = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
        print("[stt] model ready")
    return _whisper


def transcribe_pcm16(pcm_bytes: bytes) -> str:
    """Transcribe raw 16-bit LE PCM mono @16kHz to text."""
    if not pcm_bytes:
        return ""
    audio = np.frombuffer(pcm_bytes, dtype=np.int16).astype(np.float32) / 32768.0
    segments, _ = get_whisper().transcribe(audio, language=None, vad_filter=True)
    return "".join(seg.text for seg in segments).strip()


class Session:
    """One ESP32/browser connection <-> one Claude session."""

    def __init__(self, ws):
        self.ws = ws
        self.mode = "default"
        self.client: ClaudeSDKClient | None = None
        self.response_task: asyncio.Task | None = None
        self.pending_perms: dict[str, asyncio.Future] = {}
        self.audio_buf = bytearray()
        self.recording = False
        self._send_lock = asyncio.Lock()

    # ---- outbound helpers -------------------------------------------------
    async def send(self, obj: dict):
        async with self._send_lock:
            await self.ws.send(json.dumps(obj))

    async def status(self, state: str):
        await self.send({"type": "status", "state": state, "mode": self.mode})

    # ---- session lifecycle ------------------------------------------------
    async def ensure_client(self):
        if self.client is not None:
            return
        options = ClaudeAgentOptions(
            model="sonnet",
            cwd=SESSION_CWD,
            can_use_tool=self._can_use_tool,
            system_prompt=(
                "You are Claude, driven from a tiny handheld remote with a small "
                "screen. Keep responses concise and readable in short lines unless "
                "asked for detail."
            ),
        )
        self.client = ClaudeSDKClient(options=options)
        await self.client.__aenter__()

    async def close_client(self):
        if self.response_task and not self.response_task.done():
            try:
                await self.client.interrupt()
            except Exception:
                pass
            self.response_task.cancel()
        if self.client is not None:
            try:
                await self.client.__aexit__(None, None, None)
            except Exception:
                pass
            self.client = None

    async def new_session(self):
        await self.close_client()
        await self.status("ready")

    # ---- permission handling (implements the modes) -----------------------
    async def _can_use_tool(self, tool_name, input_data, context):
        mode = self.mode
        if mode == "plan":
            if tool_name in READONLY_TOOLS:
                return PermissionResultAllow()
            return PermissionResultDeny(
                message="Plan mode: don't execute changes. Present a plan instead.",
            )
        if mode == "acceptEdits" and tool_name in (EDIT_TOOLS | READONLY_TOOLS):
            return PermissionResultAllow()
        # default mode (or non-edit tool in acceptEdits): ask the remote.
        allowed = await self._ask_remote_permission(tool_name, input_data)
        if allowed:
            return PermissionResultAllow()
        return PermissionResultDeny(message="Denied from the remote.")

    async def _ask_remote_permission(self, tool_name, input_data) -> bool:
        pid = uuid.uuid4().hex[:8]
        fut = asyncio.get_event_loop().create_future()
        self.pending_perms[pid] = fut
        await self.send({
            "type": "permission_request",
            "id": pid,
            "tool": tool_name,
            "input": _short(input_data),
        })
        try:
            return await asyncio.wait_for(fut, timeout=120)
        except asyncio.TimeoutError:
            return False
        finally:
            self.pending_perms.pop(pid, None)

    def resolve_permission(self, pid: str, allow: bool):
        fut = self.pending_perms.get(pid)
        if fut and not fut.done():
            fut.set_result(allow)

    # ---- running a turn ---------------------------------------------------
    async def run_prompt(self, text: str):
        await self.ensure_client()
        # If a previous turn is still streaming, interrupt and drain it first.
        if self.response_task and not self.response_task.done():
            await self.interrupt()
        self.response_task = asyncio.create_task(self._stream_turn(text))

    async def _stream_turn(self, text: str):
        try:
            await self.status("thinking")
            await self.client.query(text)
            final_text = []
            async for message in self.client.receive_response():
                if isinstance(message, AssistantMessage):
                    for block in message.content:
                        if isinstance(block, TextBlock):
                            final_text.append(block.text)
                            await self.send({"type": "text", "delta": block.text})
                        elif isinstance(block, ToolUseBlock):
                            await self.status("running_tool")
                            await self.send({
                                "type": "tool_use",
                                "name": block.name,
                                "input": _short(block.input),
                            })
                elif isinstance(message, ResultMessage):
                    break
            await self.send({"type": "result", "text": "".join(final_text)})
            await self.status("done")
        except asyncio.CancelledError:
            raise
        except Exception as e:  # noqa: BLE001
            await self.send({"type": "error", "message": str(e)})
            await self.status("done")

    async def interrupt(self):
        if self.client is None:
            return
        try:
            await self.client.interrupt()
        except Exception:
            pass
        if self.response_task and not self.response_task.done():
            # Drain leftover messages from the interrupted turn.
            try:
                async for _ in self.client.receive_response():
                    pass
            except Exception:
                pass
        await self.status("ready")

    # ---- mode switching ---------------------------------------------------
    async def set_mode(self, mode: str):
        if mode in MODES:
            self.mode = mode
            await self.status("ready")

    async def cycle_mode(self):
        i = MODES.index(self.mode) if self.mode in MODES else 0
        await self.set_mode(MODES[(i + 1) % len(MODES)])

    # ---- push-to-talk -----------------------------------------------------
    async def audio_end(self):
        self.recording = False
        pcm = bytes(self.audio_buf)
        self.audio_buf.clear()
        try:
            text = await asyncio.to_thread(transcribe_pcm16, pcm)
        except Exception as e:  # noqa: BLE001
            await self.send({"type": "error", "message": f"STT failed: {e}"})
            return
        await self.send({"type": "transcript", "text": text})


def _short(obj, limit=400):
    """Trim tool inputs so we don't blast the tiny screen / socket."""
    try:
        s = json.dumps(obj)
    except Exception:
        s = str(obj)
    return json.loads(s) if len(s) <= limit else (s[:limit] + "…")


async def handle(ws):
    session = Session(ws)
    print("[ws] client connected")
    await session.status("ready")
    try:
        async for raw in ws:
            if isinstance(raw, (bytes, bytearray)):
                if session.recording:
                    session.audio_buf.extend(raw)
                continue
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue
            t = msg.get("type")
            if t == "prompt":
                await session.run_prompt(msg.get("text", ""))
            elif t == "interrupt":
                await session.interrupt()
            elif t == "new_session":
                await session.new_session()
            elif t == "permission":
                session.resolve_permission(msg.get("id", ""), bool(msg.get("allow")))
            elif t == "set_mode":
                await session.set_mode(msg.get("mode", "default"))
            elif t == "cycle_mode":
                await session.cycle_mode()
            elif t == "audio_start":
                session.audio_buf.clear()
                session.recording = True
            elif t == "audio_end":
                await session.audio_end()
    except websockets.ConnectionClosed:
        pass
    finally:
        print("[ws] client disconnected")
        await session.close_client()


async def main():
    if not os.getenv("ANTHROPIC_API_KEY"):
        print("[warn] ANTHROPIC_API_KEY not set — the Agent SDK needs it. "
              "Set it in bridge/.env")
    print(f"[ws] bridge listening on ws://{HOST}:{PORT}")
    async with websockets.serve(handle, HOST, PORT, max_size=None):
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
