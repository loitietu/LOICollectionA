/**
 * Mermaid 图表平移 / 缩放 / 全屏增强。
 *
 * 功能与 vitepress-plugin-mermaid-pan-zoom (MIT) 一致，但做了 SSR 兼容处理：
 * - 不在模块顶层访问 DOM
 * - svg-pan-zoom 仅在浏览器端按需动态加载
 */
import { nextTick, onMounted, onUnmounted } from 'vue'

type SvgPanZoomInstance = {
  getZoom: () => number
  resetZoom: () => void
  center: () => void
  destroy: () => void
}

type SvgPanZoomFactory = (svg: SVGSVGElement, options?: Record<string, unknown>) => SvgPanZoomInstance

let svgPanZoomFactoryPromise: Promise<SvgPanZoomFactory> | null = null

function getSvgPanZoomFactory(): Promise<SvgPanZoomFactory> {
  if (!svgPanZoomFactoryPromise) {
    svgPanZoomFactoryPromise = import('svg-pan-zoom').then(
      (module) => module.default as unknown as SvgPanZoomFactory,
    )
  }
  return svgPanZoomFactoryPromise
}

const panZoomOptions = {
  panEnabled: true,
  zoomEnabled: true,
  controlIconsEnabled: false,
  fit: true,
  center: true,
  minZoom: 0.5,
  maxZoom: 5,
}

let modalElement: HTMLDivElement | null = null
let modalPanZoomInstance: SvgPanZoomInstance | null = null
let modalZoomDisplay: HTMLDivElement | null = null

const fullscreenIconSVG =
  '<svg viewBox="0 0 24 24"><path d="M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"/></svg>'
const closeIconSVG =
  '<svg viewBox="0 0 24 24"><path d="M5 16h3v3h2v-5H5v2zm3-8H5v2h5V5H8v3zm6 11h2v-3h3v-2h-5v5zm2-11V5h-2v5h5V8h-3z"/></svg>'

function getOrCreateModal(): HTMLDivElement {
  if (modalElement) return modalElement

  const modal = document.createElement('div')
  modal.className = 'mermaid-modal-overlay'
  modal.innerHTML = `
    <div class="mermaid-modal-content">
      <div id="mermaid-modal-body"></div>
      <div class="mermaid-modal-zoom-display">100%</div>
      <button class="mermaid-modal-close-btn">${closeIconSVG}</button>
    </div>
  `
  document.body.appendChild(modal)

  modal.addEventListener('click', (event) => {
    if (event.target === modal) closeModal()
  })

  modalElement = modal
  return modal
}

async function openModal(svg: SVGSVGElement): Promise<void> {
  const modal = getOrCreateModal()
  const modalBody = modal.querySelector<HTMLDivElement>('#mermaid-modal-body')
  if (!modalBody) return

  modalBody.innerHTML = ''
  const clonedSvg = svg.cloneNode(true) as SVGSVGElement
  modalBody.appendChild(clonedSvg)
  modal.style.display = 'flex'
  document.body.style.overflow = 'hidden'

  const factory = await getSvgPanZoomFactory()
  modalZoomDisplay = modal.querySelector<HTMLDivElement>('.mermaid-modal-zoom-display')

  modalPanZoomInstance = factory(clonedSvg, {
    ...panZoomOptions,
    viewportSelector: '#mermaid-modal-body',
    onZoom: (scale: number) => {
      if (modalZoomDisplay) modalZoomDisplay.textContent = `${Math.round(scale * 100)}%`
    },
  })

  if (modalZoomDisplay && modalPanZoomInstance) {
    modalZoomDisplay.textContent = `${Math.round(modalPanZoomInstance.getZoom() * 100)}%`
  }

  const closeButton = modal.querySelector<HTMLButtonElement>('.mermaid-modal-close-btn')
  closeButton?.replaceWith(closeButton.cloneNode(true))
  const newCloseButton = modal.querySelector<HTMLButtonElement>('.mermaid-modal-close-btn')
  newCloseButton?.addEventListener('click', (event) => {
    event.stopPropagation()
    closeModal()
  })

  modalZoomDisplay?.replaceWith(modalZoomDisplay.cloneNode(true))
  const newZoomDisplay = modal.querySelector<HTMLDivElement>('.mermaid-modal-zoom-display')
  newZoomDisplay?.addEventListener('click', (event) => {
    event.stopPropagation()
    modalPanZoomInstance?.resetZoom()
    modalPanZoomInstance?.center()
    if (newZoomDisplay && modalPanZoomInstance) {
      newZoomDisplay.textContent = `${Math.round(modalPanZoomInstance.getZoom() * 100)}%`
    }
  })
  modalZoomDisplay = newZoomDisplay
}

function closeModal(): void {
  if (!modalElement) return
  modalPanZoomInstance?.destroy()
  modalPanZoomInstance = null
  modalZoomDisplay = null
  modalElement.style.display = 'none'
  document.body.style.overflow = ''
  const modalBody = modalElement.querySelector('#mermaid-modal-body')
  if (modalBody) modalBody.innerHTML = ''
}

function addControlElements(container: HTMLElement): void {
  if (container.querySelector('.mermaid-fullscreen-btn')) return

  const zoomDisplay = document.createElement('div')
  zoomDisplay.className = 'mermaid-inline-zoom-display'
  zoomDisplay.textContent = '100%'
  container.appendChild(zoomDisplay)

  const button = document.createElement('button')
  button.className = 'mermaid-fullscreen-btn'
  button.innerHTML = fullscreenIconSVG
  button.title = '全屏查看'
  button.addEventListener('click', (event) => {
    event.stopPropagation()
    const svg = container.querySelector<SVGSVGElement>('svg')
    if (svg) void openModal(svg)
  })
  container.appendChild(button)
}

async function initializeInlineMermaid(container: HTMLElement): Promise<void> {
  const svg = container.querySelector<SVGSVGElement>('svg')
  if (!svg || svg.hasAttribute('data-pan-zoom-initialized')) return

  svg.setAttribute('data-pan-zoom-initialized', 'true')
  const inlineZoomDisplay = container.querySelector<HTMLDivElement>('.mermaid-inline-zoom-display')
  const factory = await getSvgPanZoomFactory()

  const instance = factory(svg, {
    ...panZoomOptions,
    onZoom: (scale: number) => {
      if (inlineZoomDisplay) inlineZoomDisplay.textContent = `${Math.round(scale * 100)}%`
    },
  })

  if (inlineZoomDisplay && instance) {
    inlineZoomDisplay.textContent = `${Math.round(instance.getZoom() * 100)}%`
    inlineZoomDisplay.addEventListener('click', (event) => {
      event.stopPropagation()
      instance.resetZoom()
      instance.center()
      inlineZoomDisplay.textContent = `${Math.round(instance.getZoom() * 100)}%`
    })
  }
}

function processAllMermaidContainers(): void {
  document.querySelectorAll<HTMLElement>('.mermaid').forEach((container) => {
    addControlElements(container)
    void initializeInlineMermaid(container)
  })
}

const observer =
  typeof MutationObserver === 'undefined'
    ? null
    : new MutationObserver(processAllMermaidContainers)

export function useMermaidPanZoom(): void {
  onMounted(() => {
    nextTick(() => {
      processAllMermaidContainers()
      observer?.observe(document.body, { childList: true, subtree: true })
    })
  })

  onUnmounted(() => {
    observer?.disconnect()
    if (modalElement?.style.display !== 'none') closeModal()
  })
}
