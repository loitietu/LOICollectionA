<script setup lang="ts">
import DefaultTheme from 'vitepress/theme'
import PageMeta from './components/PageMeta.vue'
</script>

<template>
  <DefaultTheme.Layout>
    <template #doc-before>
      <PageMeta />
    </template>
  </DefaultTheme.Layout>
</template>
