import { useState, useEffect, useRef } from 'react'
import mqtt from 'mqtt'
import './App.css'

function App() {
  const [text, setText] = useState('')
  const [connected, setConnected] = useState(false)
  const clientRef = useRef(null)

  useEffect(() => {
    const isLocal = window.location.hostname === 'localhost';
    const c = mqtt.connect(
      isLocal ? 'ws://192.168.0.11:9001' : 'wss://mqtt.mmoors.me'
    );

    c.on("connect", () => setConnected(true))
    c.on("close", () => setConnected(false))
    c.on("error", (err) => console.error("MQTT error:", err.message))

    clientRef.current = c

    return () => c.end()
  }, [])

  const sendTextMessage = () => {
    if (!clientRef.current || !connected || !text) return
    clientRef.current.publish("adaline/keyboard/text", text)
    setText('')
  }

  const sendShortcutMessage = () => {
    if (!clientRef.current || !connected || !text) return
    clientRef.current.publish("adaline/keyboard/shortcut", text)
    setText('')
  }

  const handleKeyDown = (e) => {
    if (e.key === 'Enter') sendTextMessage()

  }

  return (
    <>
      <div className="card" style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
        <span style={{ fontSize: '12px', color: connected ? 'green' : 'red' }}>
          {connected ? '● Connected' : '● Disconnected'}
        </span>
        <input
          type="text"
          value={text}
          onChange={(e) => setText(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="Type something..."
          disabled={!connected}
        />
        <button onClick={sendTextMessage} disabled={!connected || !text}>
          Send text to Pico W
        </button>
        <button onClick={sendShortcutMessage} disabled={!connected || !text}>
          Send shortcut to Pico W
        </button>
        <button value={"GUI+R"} onClick={(e) => setText(e.target.value)}>
          WIN + R
        </button>
        <button value={"ENTER"} onClick={(e) => setText(e.target.value)}>
          ENTER
        </button>

      </div>
    </>
  )
}

export default App