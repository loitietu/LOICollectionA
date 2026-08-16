import DefaultTheme from 'vitepress/theme'
import 'vitepress-plugin-mermaid-pan-zoom/dist/style.css'
import { useMermaidPanZoom } from './mermaid-pan-zoom'
import Layout from './Layout.vue'
import './custom.css'

export default {
  extends: DefaultTheme,
  Layout,
  setup() {
    useMermaidPanZoom()
  },
}
