// `defineShikiSetup` from @slidev/types is a type-only identity helper, and
// @slidev/types is not hoisted under the Deno node_modules layout. Plain
// default export is runtime-identical.

const matrix = {
  name: 'matrix',
  type: 'dark' as const,
  colors: {
    'editor.background': '#040a04',
    'editor.foreground': '#00cc34',
  },
  settings: [
    { scope: ['comment', 'punctuation.definition.comment'], settings: { foreground: '#1f6b2c' } },
    { scope: ['keyword', 'storage', 'storage.type', 'storage.modifier'], settings: { foreground: '#00ff41' } },
    { scope: ['entity.name.function', 'support.function'], settings: { foreground: '#7dffa8' } },
    { scope: ['entity.name.type', 'support.type', 'entity.name.class'], settings: { foreground: '#00e63a' } },
    { scope: ['string', 'string.quoted'], settings: { foreground: '#5fd97a' } },
    { scope: ['constant.numeric', 'constant.language'], settings: { foreground: '#00ffa3' } },
    { scope: ['variable', 'variable.parameter'], settings: { foreground: '#00cc34' } },
    { scope: ['keyword.control.directive', 'meta.preprocessor'], settings: { foreground: '#0a8a26' } },
    { scope: ['punctuation', 'meta.brace'], settings: { foreground: '#0a8a26' } },
  ],
}

export default () => ({
  themes: {
    dark: matrix,
    light: matrix,
  },
})
