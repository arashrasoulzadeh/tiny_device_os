/* pymdownx.superfences' fence_code_format formats a mermaid fence as
 * <pre class="mermaid"><code>DIAGRAM TEXT</code></pre> - standard
 * code-block markup. mermaid.run()'s default element handling reads the
 * .mermaid element's own content directly, which includes the literal
 * nested <code>...</code> tags as text ("No diagram type detected ...
 * for text: <code>flowchart TB..."), not just the diagram source inside
 * them. Unwrap each <pre class="mermaid"> element down to its <code>
 * child's plain text content before handing it to mermaid - this is
 * also why startOnLoad:true alone never worked here, independent of
 * any DOMContentLoaded timing.
 *
 * mermaid's own `startOnLoad: true` additionally races DOMContentLoaded
 * against this script's own late position in <body> - calling
 * mermaid.run() explicitly, once the DOM is definitely ready, avoids
 * that race regardless. */
mermaid.initialize({ startOnLoad: false });

function ardubotRunMermaid() {
  document.querySelectorAll('pre.mermaid').forEach(function (pre) {
    var code = pre.querySelector('code');
    if (code) {
      pre.textContent = code.textContent;
    }
  });
  mermaid.run().catch(function (e) {
    console.error('mermaid.run() failed:', e && e.message, e);
  });
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', ardubotRunMermaid);
} else {
  ardubotRunMermaid();
}
