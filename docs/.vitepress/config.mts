import fs from 'node:fs'
import path from 'node:path'
import { defineConfig } from 'vitepress'
import { chineseSearchOptimize, pagefindPlugin } from 'vitepress-plugin-pagefind'
import lcuiGrammar from './lcui.tmLanguage.json'
import logGrammar from './log.tmLanguage.json'

export default defineConfig({
  lang: 'zh-CN',
  title: 'LOICollectionA',
  description:
    'LOICollectionA 文档 - Minecraft Bedrock Server LeviLamina Plugin 一个适用于 LeviLamina 模组加载平台的多功能插件',
  base: '/LOICollectionA/',

  head: [
    ['meta', { name: 'keywords', content: 'LOICollectionA,levilamina,minecraft,基岩版,插件' }],
    ['meta', { name: 'author', content: 'tietu' }],
  ],

  markdown: {
    // 禁用 markdown-it-attrs：避免 {version_mc} 等宏被解析为 HTML 属性而吞掉内容
    attrs: { disable: true },
    lineNumbers: false,
    theme: { light: 'github-light', dark: 'github-dark' },
    languages: [lcuiGrammar, logGrammar],
  },

  vite: {
    plugins: [
      pagefindPlugin({
        btnPlaceholder: '搜索',
        placeholder: '搜索文档',
        emptyText: '找不到结果',
        heading: '共 {{searchResult}} 条结果',
        showResultCount: true,
        customSearchQuery: chineseSearchOptimize,
      }),
    ],
  },

  buildEnd(siteConfig) {
    // GitHub Pages 需要 .nojekyll 以跳过 Jekyll 处理
    fs.writeFileSync(path.join(siteConfig.outDir, '.nojekyll'), '')
  },

  themeConfig: {
    logo: '',

    nav: [
      { text: 'Home', link: '/' },
      { text: 'Project', link: 'https://github.com/loitietu/LOICollectionA' },
    ],

    sidebar: [
      {
        text: '用户文档',
        items: [
          { text: '快速开始', link: '/md/start' },
          { text: '适配版本', link: '/md/version' },
          { text: '数据文件', link: '/md/data' },
          { text: '命令列表', link: '/md/command' },
          { text: 'LOICollectionAPI', link: '/md/api' },
          { text: '原生 UI（Native UI）', link: '/md/native-ui' },
          { text: 'LCUI 脚本语法', link: '/md/lcui' },
          { text: '常见错误含义', link: '/md/errors' },
        ],
      },
      {
        text: '开发者文档',
        items: [
          { text: '架构概览', link: '/dev/architecture' },
          { text: '模块开发指南', link: '/dev/module' },
          { text: 'API 扩展指南', link: '/dev/api-extension' },
          { text: '构建与测试', link: '/dev/build' },
          { text: '环境配置', link: '/dev/config' },
        ],
      },
      {
        text: '插件教程',
        items: [{ text: '数据迁移', link: '/course/migrate' }],
      },
    ],

    outline: { level: [2, 3], label: '本页目录' },

    docFooter: { prev: '上一卷', next: '下一卷' },

    notFound: {
      title: '404 Not Found',
      quote: '指定页面不存在 😕\n请重新刷新或者访问其它页面 😋',
      linkLabel: '返回主页',
      link: '/',
    },
  },
})
