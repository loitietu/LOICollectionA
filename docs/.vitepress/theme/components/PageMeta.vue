<script setup lang="ts">
import { computed } from 'vue'
import { useData } from 'vitepress'

const { frontmatter, lang } = useData()
const isEnglish = computed(() => lang.value.startsWith('en'))
</script>

<template>
  <div
    v-if="frontmatter.wordCount !== undefined || frontmatter.readingTime !== undefined"
    class="page-meta"
  >
    <template v-if="isEnglish">
      <span v-if="frontmatter.wordCount !== undefined" class="page-meta-item">
        {{ frontmatter.wordCount }} words
      </span>
      <span v-if="frontmatter.readingTime !== undefined" class="page-meta-item">
        {{ frontmatter.readingTime }} min read
      </span>
    </template>
    <template v-else>
      <span v-if="frontmatter.wordCount !== undefined" class="page-meta-item">
        全文约 {{ frontmatter.wordCount }} 字
      </span>
      <span v-if="frontmatter.readingTime !== undefined" class="page-meta-item">
        预计阅读 {{ frontmatter.readingTime }} 分钟
      </span>
    </template>
  </div>
</template>
