import { createContext, useContext, useState } from 'react'
import type { ReactNode } from 'react'

type Lang = 'fr' | 'en'
interface LangCtx { lang: Lang; setLang: (l: Lang) => void }

const Ctx = createContext<LangCtx>({ lang: 'fr', setLang: () => {} })

export function LangProvider({ children }: { children: ReactNode }) {
  const [lang, setLang] = useState<Lang>('fr')
  return <Ctx.Provider value={{ lang, setLang }}>{children}</Ctx.Provider>
}

export function useLang() { return useContext(Ctx) }

// Helper: pick string based on current lang
export function t(fr: string, en: string, lang: Lang) {
  return lang === 'fr' ? fr : en
}
