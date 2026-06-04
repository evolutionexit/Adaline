import { BrowserRouter, Routes, Route } from 'react-router-dom'
import { LangProvider } from '../i18n/LangContext'
import Home from './Home.jsx'
import './index.css'

export default function App() {
  return (
    <LangProvider>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<Home />} />
        </Routes>
      </BrowserRouter>
    </LangProvider>
  )
}
