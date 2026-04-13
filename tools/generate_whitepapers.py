from __future__ import annotations

import html
import subprocess
from pathlib import Path
from textwrap import dedent


ROOT = Path(__file__).resolve().parents[1]
DOCS_DIR = ROOT / "docs"

OUTPUTS = {
    "en": {
        "html": DOCS_DIR / "minidb-studio-algorithm-design-en.html",
        "pdf": DOCS_DIR / "minidb-studio-algorithm-whitepaper-en.pdf",
        "png": DOCS_DIR / "minidb-studio-cover-en.png",
    },
    "ko": {
        "html": DOCS_DIR / "minidb-studio-algorithm-design-ko.html",
        "pdf": DOCS_DIR / "minidb-studio-algorithm-whitepaper-ko.pdf",
        "png": DOCS_DIR / "minidb-studio-cover-ko.png",
    },
}


BASE_CSS = dedent(
    """
    :root {
        --bg: #111827;
        --panel: #1f2937;
        --panel-soft: rgba(31, 41, 55, 0.88);
        --border: #374151;
        --accent: #60a5fa;
        --accent-soft: rgba(96, 165, 250, 0.12);
        --text: #f9fafb;
        --muted: #9ca3af;
        --shadow: 0 18px 44px rgba(0, 0, 0, 0.26);
        --radius-xl: 30px;
        --radius-lg: 22px;
        --radius-md: 16px;
        --page-width: 1180px;
    }

    * {
        box-sizing: border-box;
    }

    html {
        background: var(--bg);
        color: var(--text);
        scroll-behavior: smooth;
    }

    body {
        margin: 0;
        color: var(--text);
        background:
            radial-gradient(circle at top right, rgba(96, 165, 250, 0.10), transparent 32%),
            radial-gradient(circle at top left, rgba(255, 255, 255, 0.03), transparent 22%),
            var(--bg);
        font-family: "Inter", "Segoe UI", system-ui, -apple-system, BlinkMacSystemFont, sans-serif;
        line-height: 1.66;
        letter-spacing: -0.01em;
        -webkit-print-color-adjust: exact;
        print-color-adjust: exact;
    }

    body.lang-ko {
        font-family: "Noto Sans KR", "Pretendard", "Malgun Gothic", "Apple SD Gothic Neo", "Segoe UI", sans-serif;
        line-height: 1.86;
        word-break: keep-all;
    }

    img, svg {
        display: block;
        max-width: 100%;
    }

    a {
        color: inherit;
        text-decoration: none;
    }

    main.paper {
        width: min(100%, var(--page-width));
        margin: 0 auto;
        padding: 42px 34px 56px;
    }

    .page {
        position: relative;
        overflow: hidden;
        border-radius: var(--radius-xl);
        border: 1px solid var(--border);
        background: linear-gradient(180deg, rgba(31, 41, 55, 0.96), rgba(17, 24, 39, 0.96));
        box-shadow: var(--shadow);
        padding: 34px;
        margin-bottom: 28px;
        break-inside: avoid;
        page-break-inside: avoid;
    }

    .page.break {
        break-before: page;
        page-break-before: always;
    }

    .page::before {
        content: "";
        position: absolute;
        inset: 0;
        pointer-events: none;
        border-radius: inherit;
        background: linear-gradient(135deg, rgba(96, 165, 250, 0.06), transparent 34%);
    }

    .cover {
        min-height: 900px;
        display: flex;
        flex-direction: column;
        justify-content: space-between;
    }

    .cover-grid {
        display: grid;
        grid-template-columns: minmax(0, 1.1fr) minmax(0, 0.9fr);
        gap: 24px;
        align-items: stretch;
    }

    .hero-block {
        display: flex;
        flex-direction: column;
        gap: 18px;
    }

    .eyebrow {
        display: inline-flex;
        align-items: center;
        gap: 10px;
        padding: 7px 12px;
        border-radius: 999px;
        border: 1px solid rgba(96, 165, 250, 0.28);
        background: rgba(96, 165, 250, 0.10);
        color: var(--accent);
        font-size: 12px;
        font-weight: 700;
        letter-spacing: 0.14em;
        text-transform: uppercase;
    }

    body.lang-ko .eyebrow {
        letter-spacing: 0.06em;
    }

    h1,
    h2,
    h3,
    h4,
    p {
        margin: 0;
    }

    .title {
        font-size: 54px;
        line-height: 1.02;
        font-weight: 800;
        letter-spacing: -0.04em;
    }

    .subtitle {
        font-size: 22px;
        color: var(--muted);
        line-height: 1.5;
        max-width: 680px;
    }

    .hero-copy {
        font-size: 15px;
        color: var(--muted);
        max-width: 640px;
    }

    body.lang-ko .title {
        font-size: 48px;
        line-height: 1.18;
    }

    body.lang-ko .subtitle {
        line-height: 1.72;
    }

    .chips {
        display: flex;
        flex-wrap: wrap;
        gap: 10px;
    }

    .chip {
        display: inline-flex;
        align-items: center;
        gap: 8px;
        padding: 9px 12px;
        border-radius: 999px;
        border: 1px solid var(--border);
        background: rgba(255, 255, 255, 0.02);
        color: var(--text);
        font-size: 13px;
        font-weight: 600;
    }

    .chip::before {
        content: "";
        width: 7px;
        height: 7px;
        border-radius: 999px;
        background: var(--accent);
        opacity: 0.9;
    }

    .hero-metrics,
    .meta-grid,
    .grid-2,
    .grid-3,
    .roadmap {
        display: grid;
        gap: 16px;
    }

    .hero-metrics {
        grid-template-columns: repeat(4, minmax(0, 1fr));
    }

    .meta-grid {
        grid-template-columns: repeat(3, minmax(0, 1fr));
        margin-top: 18px;
    }

    .grid-2 {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }

    .grid-3 {
        grid-template-columns: repeat(3, minmax(0, 1fr));
    }

    .roadmap {
        grid-template-columns: repeat(3, minmax(0, 1fr));
    }

    .card,
    .metric,
    .meta,
    .horizon {
        border-radius: var(--radius-lg);
        border: 1px solid var(--border);
        background: rgba(15, 23, 42, 0.45);
        padding: 20px;
        position: relative;
        overflow: hidden;
    }

    .card::after,
    .metric::after,
    .meta::after,
    .horizon::after {
        content: "";
        position: absolute;
        inset: 0;
        pointer-events: none;
        border-radius: inherit;
        background: linear-gradient(180deg, rgba(255, 255, 255, 0.03), transparent 34%);
    }

    .metric {
        padding: 18px 20px;
    }

    .metric-label,
    .meta-label,
    .kicker,
    .section-index,
    .table caption,
    .small-label {
        color: var(--muted);
        font-size: 12px;
        font-weight: 700;
        letter-spacing: 0.08em;
        text-transform: uppercase;
    }

    body.lang-ko .metric-label,
    body.lang-ko .meta-label,
    body.lang-ko .kicker,
    body.lang-ko .section-index,
    body.lang-ko .table caption,
    body.lang-ko .small-label {
        letter-spacing: 0.03em;
    }

    .metric-value,
    .mono {
        font-family: "JetBrains Mono", "Consolas", "SFMono-Regular", monospace;
    }

    .metric-value {
        font-size: 24px;
        line-height: 1.15;
        font-weight: 700;
        margin-top: 8px;
    }

    .meta-value {
        margin-top: 8px;
        font-size: 15px;
        font-weight: 600;
        line-height: 1.5;
    }

    .section-head {
        display: flex;
        justify-content: space-between;
        gap: 20px;
        align-items: flex-start;
        margin-bottom: 22px;
    }

    .section-head-main {
        display: flex;
        flex-direction: column;
        gap: 10px;
        max-width: 780px;
    }

    .section-index {
        color: var(--accent);
    }

    .section-title {
        font-size: 31px;
        line-height: 1.12;
        font-weight: 750;
        letter-spacing: -0.03em;
    }

    .section-lead {
        color: var(--muted);
        font-size: 15px;
        line-height: 1.72;
    }

    body.lang-ko .section-title {
        line-height: 1.28;
    }

    body.lang-ko .section-lead {
        line-height: 1.88;
    }

    .card-title {
        font-size: 18px;
        line-height: 1.3;
        font-weight: 700;
        margin-bottom: 14px;
    }

    .card-copy,
    .card p,
    .card li,
    .meta p,
    .horizon p {
        color: #d6dbe6;
        font-size: 14px;
        line-height: 1.72;
    }

    body.lang-ko .card-copy,
    body.lang-ko .card p,
    body.lang-ko .card li,
    body.lang-ko .meta p,
    body.lang-ko .horizon p {
        line-height: 1.9;
    }

    .bullet-list {
        margin: 0;
        padding: 0;
        list-style: none;
        display: grid;
        gap: 12px;
    }

    .bullet-list li {
        position: relative;
        padding-left: 18px;
    }

    .bullet-list li::before {
        content: "";
        position: absolute;
        top: 0.72em;
        left: 0;
        width: 7px;
        height: 7px;
        border-radius: 999px;
        background: var(--accent);
    }

    .note {
        padding: 14px 16px;
        border-radius: var(--radius-md);
        border: 1px solid rgba(96, 165, 250, 0.24);
        background: rgba(96, 165, 250, 0.10);
        color: #dce9ff;
        font-size: 14px;
        line-height: 1.72;
    }

    .table {
        width: 100%;
        border-collapse: separate;
        border-spacing: 0;
        overflow: hidden;
        border-radius: var(--radius-md);
        border: 1px solid var(--border);
        background: rgba(15, 23, 42, 0.44);
    }

    .table th,
    .table td {
        padding: 13px 14px;
        text-align: left;
        vertical-align: top;
        font-size: 13px;
        line-height: 1.6;
        border-bottom: 1px solid rgba(55, 65, 81, 0.62);
    }

    .table th {
        color: var(--text);
        background: rgba(255, 255, 255, 0.04);
        font-size: 12px;
        letter-spacing: 0.08em;
        text-transform: uppercase;
    }

    .table tr:last-child td {
        border-bottom: 0;
    }

    .table td.mono,
    .table th.mono {
        font-family: "JetBrains Mono", "Consolas", "SFMono-Regular", monospace;
    }

    .figure {
        display: flex;
        flex-direction: column;
        gap: 12px;
    }

    .diagram-frame {
        border-radius: 24px;
        border: 1px solid var(--border);
        background: rgba(15, 23, 42, 0.58);
        padding: 18px;
    }

    .figure-caption {
        color: var(--muted);
        font-size: 13px;
        line-height: 1.64;
    }

    .code-inline {
        display: inline-flex;
        align-items: center;
        padding: 2px 8px;
        border-radius: 999px;
        background: rgba(255, 255, 255, 0.05);
        border: 1px solid rgba(255, 255, 255, 0.08);
        font-family: "JetBrains Mono", "Consolas", "SFMono-Regular", monospace;
        font-size: 12px;
        color: #dbeafe;
        white-space: nowrap;
    }

    .quote-card {
        background: linear-gradient(180deg, rgba(96, 165, 250, 0.10), rgba(15, 23, 42, 0.22));
    }

    .horizon-title {
        font-size: 18px;
        font-weight: 700;
        margin-bottom: 12px;
    }

    .footer-summary {
        display: grid;
        grid-template-columns: 1.1fr 0.9fr;
        gap: 16px;
    }

    .cover-visual {
        height: 100%;
        display: flex;
        flex-direction: column;
        gap: 16px;
    }

    .cover-card {
        border-radius: 28px;
        border: 1px solid rgba(96, 165, 250, 0.22);
        background: linear-gradient(180deg, rgba(96, 165, 250, 0.12), rgba(15, 23, 42, 0.60));
        padding: 22px;
    }

    .cover-card-title {
        font-size: 16px;
        font-weight: 700;
        margin-bottom: 12px;
    }

    .cover-card-copy {
        color: #d6dbe6;
        font-size: 14px;
        line-height: 1.72;
    }

    .timeline {
        display: grid;
        gap: 14px;
    }

    .timeline-row {
        display: grid;
        grid-template-columns: 140px minmax(0, 1fr);
        gap: 14px;
        padding: 14px 0;
        border-top: 1px solid rgba(55, 65, 81, 0.62);
    }

    .timeline-row:first-child {
        border-top: 0;
        padding-top: 0;
    }

    .timeline-tag {
        color: var(--accent);
        font-size: 12px;
        font-weight: 700;
        letter-spacing: 0.08em;
        text-transform: uppercase;
    }

    svg .frame {
        fill: rgba(17, 24, 39, 0.88);
        stroke: #374151;
        stroke-width: 1.4;
    }

    svg .frame-soft {
        fill: rgba(255, 255, 255, 0.03);
        stroke: #374151;
        stroke-width: 1.2;
    }

    svg .accent-fill {
        fill: rgba(96, 165, 250, 0.16);
        stroke: #60a5fa;
        stroke-width: 1.5;
    }

    svg .accent-stroke {
        stroke: #60a5fa;
        stroke-width: 2;
        fill: none;
    }

    svg .muted-stroke {
        stroke: #64748b;
        stroke-width: 1.8;
        fill: none;
    }

    svg .text {
        fill: #f9fafb;
        font-size: 16px;
        font-family: "Inter", "Segoe UI", system-ui, sans-serif;
    }

    body.lang-ko svg .text {
        font-family: "Noto Sans KR", "Pretendard", "Malgun Gothic", "Apple SD Gothic Neo", sans-serif;
    }

    svg .text-muted {
        fill: #9ca3af;
        font-size: 13px;
        font-family: "Inter", "Segoe UI", system-ui, sans-serif;
    }

    body.lang-ko svg .text-muted {
        font-family: "Noto Sans KR", "Pretendard", "Malgun Gothic", "Apple SD Gothic Neo", sans-serif;
    }

    svg .text-strong {
        fill: #f9fafb;
        font-size: 16px;
        font-weight: 700;
    }

    svg .text-small {
        fill: #9ca3af;
        font-size: 12px;
    }

    svg .dot {
        fill: #60a5fa;
    }

    @page {
        size: A4;
        margin: 16mm 14mm 16mm;
    }

    @media print {
        body {
            background: var(--bg);
        }

        main.paper {
            width: auto;
            padding: 0;
        }

        .page {
            margin-bottom: 10mm;
            box-shadow: none;
        }
    }
    """
).strip()


def find_chrome() -> Path:
    candidates = [
        Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
        Path(r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"),
        Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"),
        Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Chrome or Edge executable was not found for PDF/PNG export.")


def svg_defs() -> str:
    return """
    <defs>
        <marker id="arrow-accent" markerWidth="10" markerHeight="10" refX="8" refY="5" orient="auto">
            <path d="M0,0 L10,5 L0,10 Z" fill="#60a5fa"></path>
        </marker>
        <marker id="arrow-muted" markerWidth="10" markerHeight="10" refX="8" refY="5" orient="auto">
            <path d="M0,0 L10,5 L0,10 Z" fill="#64748b"></path>
        </marker>
    </defs>
    """


def svg_text_block(x: float, y: float, lines: list[str], cls: str, line_height: int = 18, anchor: str = "start") -> str:
    if not lines:
        return ""
    escaped = [html.escape(line) for line in lines]
    out = [f'<text x="{x}" y="{y}" class="{cls}" text-anchor="{anchor}">']
    out.append(escaped[0])
    for line in escaped[1:]:
        out.append(f'<tspan x="{x}" dy="{line_height}">{line}</tspan>')
    out.append("</text>")
    return "".join(out)


def svg_box(x: float, y: float, w: float, h: float, title: str, subtitle: list[str], accent: bool = False) -> str:
    cls = "accent-fill" if accent else "frame"
    return (
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="18" class="{cls}"></rect>'
        + svg_text_block(x + 18, y + 30, [title], "text text-strong")
        + svg_text_block(x + 18, y + 56, subtitle, "text-muted", line_height=17)
    )


def svg_arrow(x1: float, y1: float, x2: float, y2: float, accent: bool = True, dashed: bool = False) -> str:
    stroke_class = "accent-stroke" if accent else "muted-stroke"
    dash = ' stroke-dasharray="6 6"' if dashed else ""
    marker = "arrow-accent" if accent else "arrow-muted"
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="{stroke_class}" marker-end="url(#{marker})"{dash}></line>'


def wrap_section(index: int, label: str, title: str, lead: str, body: str, break_before: bool = True) -> str:
    page_class = "page break" if break_before else "page"
    return dedent(
        f"""
        <section class="{page_class}">
            <header class="section-head">
                <div class="section-head-main">
                    <div class="section-index">{html.escape(label)} {index:02d}</div>
                    <h2 class="section-title">{html.escape(title)}</h2>
                    <p class="section-lead">{lead}</p>
                </div>
            </header>
            {body}
        </section>
        """
    ).strip()


def unordered(items: list[str]) -> str:
    rendered = "".join(f"<li>{item}</li>" for item in items)
    return f'<ul class="bullet-list">{rendered}</ul>'


def table(headers: list[str], rows: list[list[str]], mono_cols: set[int] | None = None) -> str:
    mono_cols = mono_cols or set()
    head_html = "".join(
        f'<th{" class=\"mono\"" if index in mono_cols else ""}>{html.escape(header)}</th>'
        for index, header in enumerate(headers)
    )
    body_rows = []
    for row in rows:
        cols = []
        for index, cell in enumerate(row):
            cls = ' class="mono"' if index in mono_cols else ""
            cols.append(f"<td{cls}>{cell}</td>")
        body_rows.append(f"<tr>{''.join(cols)}</tr>")
    return f'<table class="table"><thead><tr>{head_html}</tr></thead><tbody>{"".join(body_rows)}</tbody></table>'


def card(title: str, body: str, extra_class: str = "") -> str:
    klass = f"card {extra_class}".strip()
    return f'<article class="{klass}"><h3 class="card-title">{html.escape(title)}</h3>{body}</article>'


def metric(label: str, value: str) -> str:
    return (
        '<div class="metric">'
        f'<div class="metric-label">{html.escape(label)}</div>'
        f'<div class="metric-value">{html.escape(value)}</div>'
        "</div>"
    )


def meta_card(label: str, value: str) -> str:
    return (
        '<div class="meta">'
        f'<div class="meta-label">{html.escape(label)}</div>'
        f'<div class="meta-value">{value}</div>'
        "</div>"
    )


def architecture_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 470" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        <rect x="16" y="18" width="948" height="434" rx="26" class="frame-soft"></rect>
        {svg_box(56, 64, 180, 96, copy['ui'], copy['ui_sub'], accent=True)}
        {svg_box(292, 64, 182, 96, copy['app'], copy['app_sub'])}
        {svg_box(530, 64, 182, 96, copy['parse'], copy['parse_sub'])}
        {svg_box(768, 64, 156, 96, copy['db'], copy['db_sub'], accent=True)}

        {svg_arrow(236, 112, 292, 112)}
        {svg_arrow(474, 112, 530, 112)}
        {svg_arrow(712, 112, 768, 112)}

        {svg_box(120, 264, 178, 96, copy['hash'], copy['hash_sub'])}
        {svg_box(402, 246, 198, 116, copy['bptree'], copy['bptree_sub'], accent=True)}
        {svg_box(692, 246, 118, 116, copy['csv'], copy['csv_sub'])}
        {svg_box(826, 246, 98, 116, copy['wal'], copy['wal_sub'])}

        {svg_arrow(846, 160, 846, 246, accent=False)}
        {svg_arrow(812, 160, 812, 246, accent=False)}
        {svg_arrow(790, 160, 501, 246, accent=True)}
        {svg_arrow(790, 160, 209, 264, accent=False)}

        {svg_text_block(56, 220, [copy['boundary']], "text text-strong")}
        {svg_text_block(56, 244, [copy['boundary_sub']], "text-muted")}
    </svg>
    """


def ownership_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 340" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        {svg_box(42, 42, 180, 96, copy['database'], copy['database_sub'], accent=True)}
        {svg_box(282, 42, 176, 96, copy['records'], copy['records_sub'])}
        {svg_box(534, 30, 176, 108, copy['hash'], copy['hash_sub'])}
        {svg_box(762, 30, 176, 108, copy['bptree'], copy['bptree_sub'])}

        {svg_box(282, 202, 196, 102, copy['summary'], copy['summary_sub'])}
        {svg_box(560, 202, 176, 102, copy['result'], copy['result_sub'])}
        {svg_box(790, 202, 148, 102, copy['ui'], copy['ui_sub'])}

        {svg_arrow(222, 90, 282, 90)}
        {svg_arrow(458, 90, 534, 90, accent=False, dashed=True)}
        {svg_arrow(458, 90, 762, 90, accent=False, dashed=True)}
        {svg_arrow(380, 138, 380, 202)}
        {svg_arrow(478, 252, 560, 252)}
        {svg_arrow(736, 252, 790, 252, accent=False, dashed=True)}

        {svg_text_block(534, 164, [copy['borrow']], "text-muted")}
        {svg_text_block(282, 164, [copy['own']], "text-muted")}
    </svg>
    """


def planner_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 420" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        {svg_box(72, 54, 184, 96, copy['start'], copy['start_sub'], accent=True)}
        {svg_box(356, 38, 248, 122, copy['exact'], copy['exact_sub'])}
        {svg_box(704, 38, 192, 122, copy['hash'], copy['hash_sub'], accent=True)}
        {svg_box(356, 214, 248, 122, copy['range'], copy['range_sub'])}
        {svg_box(704, 214, 192, 122, copy['bptree'], copy['bptree_sub'], accent=True)}
        {svg_box(72, 250, 184, 86, copy['scan'], copy['scan_sub'])}

        {svg_arrow(256, 102, 356, 102)}
        {svg_arrow(604, 102, 704, 102)}
        {svg_arrow(480, 160, 480, 214, accent=False)}
        {svg_arrow(604, 274, 704, 274)}
        {svg_arrow(356, 274, 256, 294, accent=False)}

        {svg_text_block(320, 86, [copy['yes']], "text-muted")}
        {svg_text_block(486, 188, [copy['no'], copy['or_path']], "text-muted")}
        {svg_text_block(300, 292, [copy['fallback']], "text-muted")}
    </svg>
    """


def bptree_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 460" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        {svg_box(380, 36, 220, 88, copy['root'], copy['root_sub'], accent=True)}
        {svg_box(114, 178, 216, 104, copy['leaf_left'], copy['leaf_left_sub'])}
        {svg_box(382, 178, 216, 104, copy['leaf_mid'], copy['leaf_mid_sub'], accent=True)}
        {svg_box(650, 178, 216, 104, copy['leaf_right'], copy['leaf_right_sub'])}
        {svg_box(382, 330, 244, 92, copy['range'], copy['range_sub'])}

        {svg_arrow(468, 124, 222, 178, accent=False)}
        {svg_arrow(490, 124, 490, 178)}
        {svg_arrow(512, 124, 758, 178, accent=False)}
        {svg_arrow(598, 228, 650, 228)}
        {svg_arrow(330, 228, 382, 228, accent=False)}
        {svg_arrow(490, 282, 490, 330)}

        <circle cx="438" cy="228" r="8" class="dot"></circle>
        <circle cx="706" cy="228" r="8" class="dot"></circle>
        {svg_text_block(146, 306, [copy['split']], "text-muted")}
        {svg_text_block(392, 316, [copy['promote']], "text-muted")}
        {svg_text_block(674, 306, [copy['chain']], "text-muted")}
    </svg>
    """


def recovery_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 380" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        {svg_box(56, 134, 154, 92, copy['load'], copy['load_sub'], accent=True)}
        {svg_box(264, 134, 176, 92, copy['csv'], copy['csv_sub'])}
        {svg_box(494, 134, 176, 92, copy['wal'], copy['wal_sub'], accent=True)}
        {svg_box(724, 134, 194, 92, copy['swap'], copy['swap_sub'])}

        {svg_box(264, 40, 176, 64, copy['save'], copy['save_sub'])}
        {svg_box(494, 40, 176, 64, copy['checkpoint'], copy['checkpoint_sub'])}

        {svg_arrow(210, 180, 264, 180)}
        {svg_arrow(440, 180, 494, 180)}
        {svg_arrow(670, 180, 724, 180)}
        {svg_arrow(352, 104, 352, 134, accent=False)}
        {svg_arrow(582, 104, 582, 134, accent=False)}
        {svg_text_block(724, 244, [copy['isolation']], "text-muted")}
    </svg>
    """


def assistant_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 390" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        {svg_box(44, 54, 182, 98, copy['row'], copy['row_sub'])}
        {svg_box(44, 222, 182, 98, copy['plan'], copy['plan_sub'])}
        {svg_box(312, 136, 216, 104, copy['assistant'], copy['assistant_sub'], accent=True)}
        {svg_box(612, 136, 162, 104, copy['editor'], copy['editor_sub'])}
        {svg_box(814, 136, 122, 104, copy['parser'], copy['parser_sub'])}

        {svg_arrow(226, 104, 312, 168, accent=False)}
        {svg_arrow(226, 272, 312, 208)}
        {svg_arrow(528, 188, 612, 188)}
        {svg_arrow(774, 188, 814, 188)}
        {svg_text_block(612, 256, [copy['shared']], "text-muted")}
        {svg_text_block(44, 344, [copy['note']], "text-muted")}
    </svg>
    """


def sync_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 410" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        {svg_box(52, 132, 148, 96, copy['action'], copy['action_sub'])}
        {svg_box(244, 132, 198, 96, copy['execute'], copy['execute_sub'], accent=True)}
        {svg_box(486, 132, 188, 96, copy['engine'], copy['engine_sub'])}
        {svg_box(716, 132, 214, 96, copy['summary'], copy['summary_sub'])}

        {svg_box(244, 286, 164, 72, copy['grid'], copy['grid_sub'])}
        {svg_box(430, 286, 164, 72, copy['metrics'], copy['metrics_sub'])}
        {svg_box(616, 286, 164, 72, copy['inspector'], copy['inspector_sub'])}
        {svg_box(802, 286, 128, 72, copy['status'], copy['status_sub'])}

        {svg_arrow(200, 180, 244, 180)}
        {svg_arrow(442, 180, 486, 180)}
        {svg_arrow(674, 180, 716, 180)}
        {svg_arrow(335, 228, 326, 286, accent=False)}
        {svg_arrow(560, 228, 512, 286, accent=False)}
        {svg_arrow(790, 228, 698, 286)}
        {svg_arrow(860, 228, 866, 286, accent=False)}
    </svg>
    """


def metrics_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 360" role="img" aria-label="{html.escape(copy['aria'])}">
        <defs>
            <linearGradient id="latency-fill" x1="0%" y1="0%" x2="0%" y2="100%">
                <stop offset="0%" stop-color="#60a5fa" stop-opacity="0.45"></stop>
                <stop offset="100%" stop-color="#60a5fa" stop-opacity="0.02"></stop>
            </linearGradient>
        </defs>
        <rect x="20" y="20" width="940" height="320" rx="24" class="frame-soft"></rect>
        <text x="42" y="62" class="text text-strong">{html.escape(copy['latency'])}</text>
        <text x="516" y="62" class="text text-strong">{html.escape(copy['usage'])}</text>

        <polyline fill="url(#latency-fill)" stroke="none" points="46,208 98,188 150,206 202,148 254,162 306,114 358,126 410,92 462,116 462,252 46,252"></polyline>
        <polyline fill="none" stroke="#60a5fa" stroke-width="3" points="46,208 98,188 150,206 202,148 254,162 306,114 358,126 410,92 462,116"></polyline>
        <line x1="46" y1="252" x2="462" y2="252" stroke="#374151" stroke-width="1.4"></line>
        <text x="46" y="278" class="text-small">{html.escape(copy['latency_axis'])}</text>

        <rect x="530" y="212" width="72" height="40" rx="12" class="frame"></rect>
        <rect x="626" y="172" width="72" height="80" rx="12" class="accent-fill"></rect>
        <rect x="722" y="146" width="72" height="106" rx="12" class="frame"></rect>
        <rect x="818" y="118" width="72" height="134" rx="12" class="frame"></rect>
        <text x="548" y="278" class="text-small">{html.escape(copy['cache'])}</text>
        <text x="640" y="278" class="text-small">{html.escape(copy['hash'])}</text>
        <text x="732" y="278" class="text-small">{html.escape(copy['bptree'])}</text>
        <text x="824" y="278" class="text-small">{html.escape(copy['full'])}</text>
        <text x="516" y="312" class="text-muted">{html.escape(copy['footer'])}</text>
    </svg>
    """


def complexity_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 980 350" role="img" aria-label="{html.escape(copy['aria'])}">
        <rect x="24" y="18" width="932" height="314" rx="24" class="frame-soft"></rect>
        <text x="42" y="56" class="text text-strong">{html.escape(copy['title'])}</text>

        <line x1="180" y1="84" x2="180" y2="286" stroke="#374151" stroke-width="1.2"></line>
        <line x1="180" y1="286" x2="914" y2="286" stroke="#374151" stroke-width="1.2"></line>

        <text x="42" y="110" class="text-small">{html.escape(copy['rows'][0])}</text>
        <text x="42" y="152" class="text-small">{html.escape(copy['rows'][1])}</text>
        <text x="42" y="194" class="text-small">{html.escape(copy['rows'][2])}</text>
        <text x="42" y="236" class="text-small">{html.escape(copy['rows'][3])}</text>
        <text x="42" y="278" class="text-small">{html.escape(copy['rows'][4])}</text>

        <rect x="198" y="92" width="148" height="18" rx="9" class="frame"></rect>
        <rect x="198" y="134" width="118" height="18" rx="9" class="accent-fill"></rect>
        <rect x="198" y="176" width="202" height="18" rx="9" class="frame"></rect>
        <rect x="198" y="218" width="248" height="18" rx="9" class="accent-fill"></rect>
        <rect x="198" y="260" width="322" height="18" rx="9" class="frame"></rect>

        <text x="544" y="106" class="text-small">{html.escape(copy['labels'][0])}</text>
        <text x="544" y="148" class="text-small">{html.escape(copy['labels'][1])}</text>
        <text x="544" y="190" class="text-small">{html.escape(copy['labels'][2])}</text>
        <text x="544" y="232" class="text-small">{html.escape(copy['labels'][3])}</text>
        <text x="544" y="274" class="text-small">{html.escape(copy['labels'][4])}</text>
    </svg>
    """


def cover_visual_svg(copy: dict[str, str]) -> str:
    return f"""
    <svg viewBox="0 0 520 420" role="img" aria-label="{html.escape(copy['aria'])}">
        {svg_defs()}
        <rect x="18" y="18" width="484" height="384" rx="32" class="frame-soft"></rect>
        {svg_box(62, 52, 396, 78, copy['shell'], copy['shell_sub'], accent=True)}
        {svg_box(62, 154, 396, 78, copy['planner'], copy['planner_sub'])}
        {svg_box(62, 256, 186, 98, copy['hash'], copy['hash_sub'])}
        {svg_box(272, 256, 186, 98, copy['bptree'], copy['bptree_sub'], accent=True)}
        {svg_arrow(260, 130, 260, 154)}
        {svg_arrow(156, 232, 156, 256, accent=False)}
        {svg_arrow(364, 232, 364, 256)}
        {svg_text_block(62, 386, [copy['footer']], "text-muted")}
    </svg>
    """


COPY: dict[str, dict[str, object]] = {
    "en": {
        "html_lang": "en",
        "body_class": "",
        "document_title": "MiniDB Studio Algorithm Whitepaper (English)",
        "section_label": "Section",
        "cover_eyebrow": "Algorithm Whitepaper",
        "cover_title": "MiniDB Studio",
        "cover_subtitle": "A bilingual storage-engine and desktop database studio whitepaper for a pure C core, rule-based query planning, B+ Tree traversal, WAL recovery, and state-driven desktop observability.",
        "cover_copy": "This package documents the current architecture of MiniDB Studio as it exists in the codebase today: reusable engine modules, index-aware query execution, deterministic assistant suggestions, and raylib-based desktop synchronization on top of the same executor boundary.",
        "cover_chips": ["Pure C11 Core", "Hash Index", "B+ Tree", "CSV Snapshot", "WAL Replay", "raylib Desktop"],
        "cover_metrics": [("Engine Boundary", "Reusable"), ("Index Paths", "3"), ("Recovery Model", "CSV + WAL"), ("Document Set", "EN + KO")],
        "cover_meta": [
            ("Audience", "Engineering portfolio reviewers, interviewers, and systems-programming peers."),
            ("Scope", "Parser, planner, execution engine, recovery, desktop synchronization, and performance telemetry."),
            ("Positioning", "A lightweight database management studio with explainable internals and demo-ready visuals."),
        ],
        "cover_visual": {
            "aria": "MiniDB Studio cover architecture stack",
            "shell": "Desktop Studio Shell",
            "shell_sub": ["raylib panels", "query editor", "result grid"],
            "planner": "Shared Executor Boundary",
            "planner_sub": ["parser", "planner", "CRUD dispatch"],
            "hash": "Hash + WAL",
            "hash_sub": ["exact match", "append log"],
            "bptree": "B+ Tree + Metrics",
            "bptree_sub": ["range scan", "ordered traversal"],
            "footer": "One code path, two user surfaces: terminal + desktop.",
        },
        "cover_card_title": "Architecture Snapshot",
        "cover_card_copy": "A shared execution core keeps query semantics, recovery rules, and telemetry consistent across terminal and desktop surfaces.",
        "sections": [
            {
                "title": "Project Overview",
                "lead": "MiniDB Studio combines an in-memory record engine, rule-based optimizer, and desktop control surface without splitting the storage core from the UI shell. The result is a portfolio project that behaves like a miniature database studio rather than a parser demo.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "System Scope",
                        unordered(
                            [
                                "A <span class=\"code-inline\">C11</span> storage core owns records, indexes, recovery state, and execution statistics.",
                                "Both the terminal console and the raylib desktop app reuse the same <span class=\"code-inline\">parse -> plan -> execute</span> boundary.",
                                "Exact lookups are accelerated through hash indexes, while ordered access and range scans flow through B+ Trees.",
                                "Persistence is modeled as snapshot + log replay, which makes recovery explainable in a short interview walkthrough.",
                            ]
                        ),
                    )
                    + card(
                        "Module Inventory",
                        table(
                            ["Module", "Primary responsibility", "Representative symbols"],
                            [
                                ["<span class=\"mono\">parser.c</span>", "Tokenization, validation, and command decoding.", "<span class=\"mono\">parse_select</span>, <span class=\"mono\">parse_update</span>"],
                                ["<span class=\"mono\">planner.c</span>", "Rule-based access-path selection and sort pushdown checks.", "<span class=\"mono\">planner_choose_plan</span>"],
                                ["<span class=\"mono\">database.c</span>", "CRUD dispatch, filter evaluation, index maintenance, and query statistics.", "<span class=\"mono\">database_select</span>, <span class=\"mono\">database_estimate_memory_usage</span>"],
                                ["<span class=\"mono\">bptree.c</span>", "Ordered access and range traversal.", "<span class=\"mono\">bptree_insert</span>, <span class=\"mono\">bptree_collect_stats</span>"],
                                ["<span class=\"mono\">wal.c</span>", "Append-only mutation logging and replay.", "<span class=\"mono\">wal_append_insert</span>, <span class=\"mono\">wal_replay</span>"],
                                ["<span class=\"mono\">studio_app.c</span>", "Desktop state transitions and metric streaming.", "<span class=\"mono\">studio_execute_text</span>, <span class=\"mono\">studio_push_metrics</span>"],
                            ],
                        ),
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Core Storage Engine Architecture",
                "lead": "The most important design choice is ownership isolation. Record lifetime stays inside <span class=\"code-inline\">Database</span>, secondary indexes only borrow <span class=\"code-inline\">Record*</span> references, and UI-facing summaries materialize temporary result rows so the desktop layer never becomes the owner of storage memory.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Execution Layers",
                        '<div class="figure"><div class="diagram-frame">'
                        + architecture_svg(
                            {
                                "aria": "Core architecture layers",
                                "ui": "Desktop + Terminal",
                                "ui_sub": ["raylib Studio", "CLI shell"],
                                "app": "AppContext",
                                "app_sub": ["history", "logger", "cache"],
                                "parse": "Parser + Planner",
                                "parse_sub": ["tokenize", "predicate decode", "access path"],
                                "db": "Database",
                                "db_sub": ["records", "filters", "stats"],
                                "hash": "Hash Index",
                                "hash_sub": ["id", "department"],
                                "bptree": "Ordered Index Layer",
                                "bptree_sub": ["B+ Tree(id)", "B+ Tree(age)", "range scan", "ORDER BY age"],
                                "csv": "CSV Snapshot",
                                "csv_sub": ["SAVE", "LOAD"],
                                "wal": "WAL",
                                "wal_sub": ["INSERT", "UPDATE", "DELETE"],
                                "boundary": "Shared engine boundary",
                                "boundary_sub": "UI surfaces remain callers; the engine keeps mutation and ownership rules.",
                            }
                        )
                        + '</div><p class="figure-caption">The same execution path serves the terminal and the desktop studio. That separation preserves the pure C engine while allowing new presentation layers.</p></div>',
                    )
                    + card(
                        "Memory Ownership Model",
                        '<div class="figure"><div class="diagram-frame">'
                        + ownership_svg(
                            {
                                "aria": "Memory ownership model",
                                "database": "Database",
                                "database_sub": ["owns record lifetime", "destroys indexes on exit"],
                                "records": "Record nodes",
                                "records_sub": ["id, name, age, department", "single source of truth"],
                                "hash": "Hash buckets",
                                "hash_sub": ["borrow Record*", "never free records"],
                                "bptree": "B+ Tree value lists",
                                "bptree_sub": ["borrow Record*", "leaf-level fan-out"],
                                "summary": "CommandExecutionSummary",
                                "summary_sub": ["owns temporary", "query result materialization"],
                                "result": "QueryResult rows[]",
                                "result_sub": ["UI-safe copies", "cacheable summary payload"],
                                "ui": "Studio UI",
                                "ui_sub": ["reads only", "selection + sort state"],
                                "borrow": "Dashed arrows = borrowed references",
                                "own": "Solid arrows = owning lifetime",
                            }
                        )
                        + '</div><p class="figure-caption">This ownership split is what makes LOAD safe: a temporary database can be built, validated, and only then swapped into <span class="code-inline">AppContext</span>.</p></div>'
                        + '<div class="note">Design invariant: mutation paths update indexes and persistence metadata inside the engine before UI code reads the new state. The raylib layer never frees storage objects directly.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Query Planner & Optimizer Algorithm",
                "lead": "The optimizer is intentionally lightweight and explainable. It does not rely on a cost model; instead, it applies deterministic rules that map query shape to the best available access path, then annotates the UI with the chosen plan string.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Planner Decision Flow",
                        '<div class="figure"><div class="diagram-frame">'
                        + planner_svg(
                            {
                                "aria": "Planner flowchart",
                                "start": "Parsed SELECT",
                                "start_sub": ["predicates", "ORDER BY", "LIMIT"],
                                "exact": "Exact indexed predicate?",
                                "exact_sub": ["id = x", "department = x", "single-key equality"],
                                "hash": "Hash Exact Lookup",
                                "hash_sub": ["lowest-latency", "exact match path"],
                                "range": "Range or ordered age path?",
                                "range_sub": ["age > x", "age >= x", "ORDER BY age LIMIT k"],
                                "bptree": "B+ Tree Range Scan",
                                "bptree_sub": ["ordered leaf walk", "limit pushdown"],
                                "scan": "Full Scan",
                                "scan_sub": ["predicate filter", "optional in-memory sort"],
                                "yes": "yes",
                                "no": "no",
                                "or_path": "or OR-path needs fallback filtering",
                                "fallback": "fallback if no suitable index is available",
                            }
                        )
                        + '</div><p class="figure-caption">The planner emits plan labels such as <span class="code-inline">B+ Tree Range Scan</span> or <span class="code-inline">Hash Exact Lookup</span>, which are surfaced directly in the desktop inspector.</p></div>',
                    )
                    + card(
                        "Rule Set and Planner Effects",
                        table(
                            ["Query shape", "Primary plan", "Planner side effect"],
                            [
                                ["<span class=\"mono\">SELECT WHERE id = 42</span>", "Hash exact lookup or B+ exact lookup", "Scans a single bucket or key slot and reports a low row scan count."],
                                ["<span class=\"mono\">SELECT WHERE department = Oncology</span>", "Hash exact lookup", "Uses the department hash index and filters only the candidate chain."],
                                ["<span class=\"mono\">SELECT WHERE age &gt; 30</span>", "B+ Tree range scan", "Starts at the first qualifying leaf and walks the linked leaf chain."],
                                ["<span class=\"mono\">SELECT ORDER BY age LIMIT 10</span>", "B+ Tree ordered scan", "Suppresses extra sort work when the ordered leaf stream already matches the requested ordering."],
                                ["Mixed <span class=\"mono\">AND</span> predicate", "Indexed seed + post-filter", "Uses the best seed path, then evaluates remaining predicates in memory."],
                                ["Unsupported or broad <span class=\"mono\">OR</span>", "Full scan", "Falls back to a complete record walk so correctness is preserved."],
                            ],
                        )
                        + '<div class="note">The planner also decides whether a later in-memory sort is still required. If leaf traversal already provides the requested <span class="code-inline">ORDER BY age</span>, the engine avoids extra reorder work.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "B+ Tree Traversal Logic",
                "lead": "The ordered index layer exists to make age-based range scans and ordered traversal feel like storage-engine work rather than linear filtering. The tree keeps all records in leaves, stores separator keys in internal nodes, and links leaves through a forward chain for fast sequential access.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Split and Range Path",
                        '<div class="figure"><div class="diagram-frame">'
                        + bptree_svg(
                            {
                                "aria": "B+ Tree split and range traversal",
                                "root": "Internal node",
                                "root_sub": ["separator keys route lookups", "records stay in leaves"],
                                "leaf_left": "Leaf A",
                                "leaf_left_sub": ["22, 24, 27", "before split"],
                                "leaf_mid": "Leaf B",
                                "leaf_mid_sub": ["31, 34", "first qualifying leaf"],
                                "leaf_right": "Leaf C",
                                "leaf_right_sub": ["38, 42, 47", "linked by next"],
                                "range": "Range scan",
                                "range_sub": ["seek first age >= 31", "walk leaf->next until stop"],
                                "split": "Temporary m+1 array",
                                "promote": "Promote first key of right leaf",
                                "chain": "Leaf chain supports ordered traversal",
                            }
                        )
                        + '</div><p class="figure-caption">The current implementation comments the split logic directly in <span class="code-inline">bptree.c</span> so leaf and internal promotion rules are visible to reviewers.</p></div>',
                    )
                    + card(
                        "Operational Semantics",
                        unordered(
                            [
                                "<span class=\"code-inline\">bptree_insert</span> descends to the correct leaf, inserts in sorted order, and bubbles a split upward only when a node overflows.",
                                "Leaf split logic uses a temporary array with one extra slot, keeps the lower half in place, moves the upper half into a new right sibling, and promotes the first key of that right sibling.",
                                "Internal split logic promotes the middle separator upward while redistributing children so parent routing stays correct after the split.",
                                "Non-unique age keys are stored as value lists attached to a single leaf key, which keeps duplicate ages ordered without duplicating separators.",
                                "Deletion uses borrow-or-merge rebalancing helpers so underflowing nodes repair themselves before the tree returns control to the caller.",
                            ]
                        )
                        + table(
                            ["Operation", "Typical complexity", "Notes"],
                            [
                                ["Exact lookup", "<span class=\"mono\">O(log_B n)</span>", "Descend through separators to one leaf."],
                                ["Range scan", "<span class=\"mono\">O(log_B n + k)</span>", "Seek once, then stream through leaf links."],
                                ["Insert", "<span class=\"mono\">O(log_B n)</span>", "Split only on overflowing nodes."],
                                ["Delete", "<span class=\"mono\">O(log_B n)</span>", "Borrow/merge on underflow."],
                            ],
                            mono_cols={1},
                        ),
                    )
                    + "</div>"
                ),
            },
            {
                "title": "WAL and Snapshot Recovery Flow",
                "lead": "Persistence is modeled as a checkpointable snapshot plus an append-only mutation log. This keeps on-disk behavior easy to reason about: SAVE emits a stable CSV image, and startup recovery replays any later mutations from the WAL.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Recovery Sequence",
                        '<div class="figure"><div class="diagram-frame">'
                        + recovery_svg(
                            {
                                "aria": "Snapshot and WAL recovery flow",
                                "load": "LOAD command",
                                "load_sub": ["create temporary database"],
                                "csv": "CSV snapshot",
                                "csv_sub": ["persistence_load_csv"],
                                "wal": "WAL replay",
                                "wal_sub": ["wal_replay", "INSERT | UPDATE | DELETE"],
                                "swap": "Swap live state",
                                "swap_sub": ["replace AppContext database", "refresh status + memory"],
                                "save": "SAVE command",
                                "save_sub": ["persistence_save_csv"],
                                "checkpoint": "Checkpoint WAL",
                                "checkpoint_sub": ["truncate log after durable snapshot"],
                                "isolation": "Live state is replaced only after the temporary restore succeeds end-to-end.",
                            }
                        )
                        + '</div><p class="figure-caption">The executor loads into a temporary <span class="code-inline">Database</span>, replays <span class="code-inline">data/db.log</span>, and only then swaps the restored engine into the active context.</p></div>',
                    )
                    + card(
                        "Recovery Guarantees",
                        table(
                            ["Phase", "Current algorithm", "Why it matters"],
                            [
                                ["Mutation logging", "Append <span class=\"mono\">INSERT|...</span>, <span class=\"mono\">UPDATE|...</span>, or <span class=\"mono\">DELETE|...</span> records.", "Every change after the last snapshot has a replayable representation."],
                                ["Snapshot save", "Write all rows to CSV, then checkpoint the WAL.", "The CSV becomes a concise restore point and the log stays bounded."],
                                ["Restore", "Load CSV into a temporary engine, replay WAL, rebuild indexes as records are inserted.", "Indexes and row store are restored through the same code paths used in normal execution."],
                                ["Activation", "Assign the restored database into <span class=\"mono\">AppContext</span> only after success.", "Prevents partial recovery from corrupting the active session."],
                            ],
                        )
                        + '<div class="note">Because replay calls the normal database mutation APIs, index consistency is not a separate recovery step; it is a side effect of reusing the same insert/update/delete logic.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "AI Query Assistant Parsing Flow",
                "lead": "The current desktop assistant is intentionally deterministic. It is not an LLM runtime; instead, it synthesizes valid SQL-like suggestions from row context, prior execution plans, and canned demo heuristics, then routes those suggestions back through the same parser used for typed text.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Current Assistant Pipeline",
                        '<div class="figure"><div class="diagram-frame">'
                        + assistant_svg(
                            {
                                "aria": "Assistant suggestion flow",
                                "row": "Selected row",
                                "row_sub": ["id, department, age context"],
                                "plan": "Last plan feedback",
                                "plan_sub": ["full scan warning", "index-aware hint"],
                                "assistant": "studio_build_assistant_content",
                                "assistant_sub": ["compose valid query suggestions", "preserve engine grammar"],
                                "editor": "SQL editor",
                                "editor_sub": ["insert suggestion", "Ctrl+Enter execute"],
                                "parser": "Shared parser",
                                "parser_sub": ["normal parse/plan path"],
                                "shared": "Suggested commands are first-class inputs, not a special execution path.",
                                "note": "This keeps the assistant honest: every suggestion must compile under the same parser contract as manual queries.",
                            }
                        )
                        + '</div><p class="figure-caption">Examples include <span class="code-inline">SELECT WHERE id = ...</span> and <span class="code-inline">SELECT WHERE age &gt; ... ORDER BY age LIMIT 10</span> when a row or plan indicates that the B+ Tree would be a good fit.</p></div>',
                    )
                    + card(
                        "Assistant Design Contract",
                        unordered(
                            [
                                "Selected-row context seeds deterministic query templates that are guaranteed to be syntactically valid for the current grammar.",
                                "Recent execution plans bias suggestions toward indexed alternatives when a previous command triggered a full scan.",
                                "The editor remains the visible source of truth, so users can inspect or modify a generated command before execution.",
                                "Because assistant output flows back through <span class=\"code-inline\">parse_command</span>, validation, logging, benchmarking, and caching all continue to work unchanged.",
                            ]
                        )
                        + '<div class="note">Future natural-language parsing belongs in the roadmap, not in the current-state architecture claim. The existing implementation already creates a useful “AI assistant” surface by turning engine state into context-aware query suggestions.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Desktop UI-State Synchronization",
                "lead": "The desktop application stays responsive because UI state is thin and derived. Query execution mutates engine state inside <span class=\"code-inline\">AppContext</span>, then the desktop layer consumes summarized outputs for the editor, grid, inspector, metrics dashboard, and footer.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "State Transition Graph",
                        '<div class="figure"><div class="diagram-frame">'
                        + sync_svg(
                            {
                                "aria": "Desktop UI synchronization flow",
                                "action": "User action",
                                "action_sub": ["type, click, shortcut"],
                                "execute": "studio_execute_text",
                                "execute_sub": ["run input", "reset selection state"],
                                "engine": "app_context_run_input",
                                "engine_sub": ["parse", "plan", "execute", "log"],
                                "summary": "CommandExecutionSummary",
                                "summary_sub": ["message", "result rows", "stats", "plan"],
                                "grid": "Result grid",
                                "grid_sub": ["rows, sort, page"],
                                "metrics": "Metric buffers",
                                "metrics_sub": ["latency, scans, memory"],
                                "inspector": "Inspector",
                                "inspector_sub": ["plan, index, benchmark"],
                                "status": "Status bar",
                                "status_sub": ["records, storage, queries"],
                            }
                        )
                        + '</div><p class="figure-caption">The desktop shell does not rebuild storage views from scratch. Instead, it reacts to a compact execution summary and a snapshot of dashboard status from the engine.</p></div>',
                    )
                    + card(
                        "Synchronization Points",
                        table(
                            ["UI event", "Primary function", "Resulting state change"],
                            [
                                ["Execute query", "<span class=\"mono\">studio_execute_text</span>", "Runs the command, refreshes the latest summary, and clears stale row-selection state."],
                                ["Engine dispatch", "<span class=\"mono\">app_context_run_input</span>", "Updates cache counters, history, logger, and summary payload in one boundary."],
                                ["Metrics update", "<span class=\"mono\">studio_push_metrics</span>", "Appends latency, rows scanned, memory usage, cache ratio, and chosen plan counters."],
                                ["Grid sort", "<span class=\"mono\">result_grid_sort</span>", "Reorders already-materialized result rows without mutating the underlying engine tables."],
                                ["Footer refresh", "<span class=\"mono\">app_context_snapshot_status</span>", "Updates record count, memory estimate, and session query totals."],
                            ],
                        )
                        + '<div class="note">This flow is why the studio can stay visually rich without turning into a second storage layer. The UI owns presentation state; the engine owns truth.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Performance Dashboard Metrics",
                "lead": "MiniDB Studio exposes internal engine behavior as presentation-friendly telemetry. Each executed command updates a compact set of metric series so the desktop dashboard can explain not just results, but also the cost profile of how those results were produced.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Metric Data Flow",
                        '<div class="figure"><div class="diagram-frame">'
                        + metrics_svg(
                            {
                                "aria": "Performance metric sparkline and usage chart",
                                "latency": "Latency trend",
                                "usage": "Index usage frequency",
                                "latency_axis": "Recent query sequence",
                                "cache": "Cache",
                                "hash": "Hash",
                                "bptree": "B+",
                                "full": "Scan",
                                "footer": "Each execution updates latency, rows scanned, memory, cache ratio, and selected-plan counters.",
                            }
                        )
                        + '</div><p class="figure-caption">The dashboard visualizes engine-side signals that already exist in execution summaries, so the desktop layer is reading metrics rather than inventing them.</p></div>',
                    )
                    + card(
                        "Metric Definitions",
                        table(
                            ["Metric", "Source field", "Operational meaning"],
                            [
                                ["Query latency trend", "<span class=\"mono\">last_elapsed_ms</span>", "Wall-clock duration for the most recent command path."],
                                ["Rows scanned", "<span class=\"mono\">summary.stats.rows_scanned</span>", "How many records or index candidates were touched to answer the query."],
                                ["Memory usage", "<span class=\"mono\">database_estimate_memory_usage</span>", "Approximate live storage footprint across records, indexes, and B+ Tree value nodes."],
                                ["Cache hit ratio", "<span class=\"mono\">query_cache_hits / query_cache_lookups</span>", "How often repeatable commands are served from the session cache."],
                                ["Index usage frequency", "Plan counters keyed by chosen plan", "How often hash, B+ Tree, cache, or full-scan paths are selected during the session."],
                            ],
                        )
                        + '<div class="note">The dashboard is intentionally lightweight: tiny chart buffers, no heavy telemetry subsystem, and all values derived from already-existing execution accounting.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Complexity Analysis",
                "lead": "The engine is small enough to explain asymptotically, but rich enough to show real trade-offs between exact lookup structures, ordered traversals, and recovery work. The chart below uses relative bar lengths to show the operational cost profile reviewers should expect.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Relative Cost Profile",
                        '<div class="figure"><div class="diagram-frame">'
                        + complexity_svg(
                            {
                                "aria": "Relative complexity chart",
                                "title": "Relative operation cost",
                                "rows": [
                                    "Parse and tokenize",
                                    "Hash exact lookup",
                                    "B+ Tree exact lookup",
                                    "B+ Tree range scan",
                                    "Snapshot load + WAL replay",
                                ],
                                "labels": [
                                    "O(L)",
                                    "Avg O(1), worst O(n)",
                                    "O(log_B n)",
                                    "O(log_B n + k)",
                                    "O(n) + O(m log_B n)",
                                ],
                            }
                        )
                        + '</div><p class="figure-caption">Range scans become attractive when result ordering matters, even if the point lookup case is faster in the hash index.</p></div>',
                    )
                    + card(
                        "Complexity Table",
                        table(
                            ["Operation", "Complexity", "Why it matters"],
                            [
                                ["Tokenize + parse", "<span class=\"mono\">O(L)</span>", "Linear in query text length; keeps the front end predictable."],
                                ["Hash exact lookup", "<span class=\"mono\">O(1)</span> average", "Best path for direct id or department equality filters."],
                                ["B+ Tree exact lookup", "<span class=\"mono\">O(log_B n)</span>", "Stable ordered search with a small tree height."],
                                ["B+ Tree range scan", "<span class=\"mono\">O(log_B n + k)</span>", "One seek followed by leaf-chain streaming."],
                                ["Insert / delete with index maintenance", "<span class=\"mono\">O(log_B n)</span>", "Mutation cost is dominated by B+ Tree maintenance after record lookup/allocation."],
                                ["Snapshot load", "<span class=\"mono\">O(n)</span>", "All rows are reconstructed through normal insert paths."],
                                ["WAL replay", "<span class=\"mono\">O(m log_B n)</span>", "Recovery cost scales with replay length and index rebuild work."],
                                ["Result-grid sort fallback", "<span class=\"mono\">O(r log r)</span>", "Used only when the planner cannot preserve the desired order natively."],
                            ],
                            mono_cols={1},
                        ),
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Future Scaling Roadmap",
                "lead": "The current architecture is deliberately compact, but its seams already point toward larger storage-engine work. The roadmap below keeps the pure C core intact while extending it toward disk management, richer planning, and more capable assistant behavior.",
                "body": lambda c: (
                    '<div class="roadmap">'
                    + "".join(
                        [
                            '<article class="horizon"><div class="small-label">Near-term</div><h3 class="horizon-title">Local engine depth</h3><p>Introduce slotted-page serialization, richer index statistics, and more selective predicate normalization while preserving the current in-memory API surface.</p></article>',
                            '<article class="horizon"><div class="small-label">Mid-term</div><h3 class="horizon-title">Storage realism</h3><p>Add a buffer-pool abstraction, asynchronous checkpointing, composite indexes, and more explicit recovery metadata so SAVE/LOAD can grow into page-oriented persistence.</p></article>',
                            '<article class="horizon"><div class="small-label">Long-term</div><h3 class="horizon-title">DB studio evolution</h3><p>Layer MVCC, a cost-based optimizer, and validated natural-language-to-query conversion on top of the existing parser boundary without rewriting the storage engine.</p></article>',
                        ]
                    )
                    + '<div class="card quote-card"><h3 class="card-title">Roadmap Principle</h3><p>Every next step should keep the current strength of the project intact: explainable internals, reusable boundaries, and a UI that visualizes real engine behavior instead of hiding it.</p></div>'
                ),
            },
            {
                "title": "Final Engineering Summary",
                "lead": "MiniDB Studio succeeds as a portfolio system because the engine, optimizer, recovery path, and desktop shell tell one coherent story. It is small enough to present in a few minutes and deep enough to discuss indexing, ownership, and execution trade-offs with confidence.",
                "body": lambda c: (
                    '<div class="footer-summary">'
                    + card(
                        "Why the design holds together",
                        '<div class="timeline">'
                        + "".join(
                            [
                                '<div class="timeline-row"><div class="timeline-tag">Reusable core</div><div>The same parser, planner, and database code power both the console interface and the desktop studio, which makes the architecture feel intentional rather than forked.</div></div>',
                                '<div class="timeline-row"><div class="timeline-tag">Explainable performance</div><div>Hash lookups, B+ Tree traversals, scan counts, and cache hits all surface clearly enough to discuss the query path chosen for a single command.</div></div>',
                                '<div class="timeline-row"><div class="timeline-tag">Recovery clarity</div><div>CSV snapshots plus WAL replay create a clean mental model for durability, restore safety, and index reconstruction through normal mutation paths.</div></div>',
                            ]
                        )
                        + "</div>",
                    )
                    + card(
                        "Portfolio framing",
                        '<p class="card-copy">MiniDB Studio demonstrates the kind of engineering judgment interviewers look for in systems work: explicit ownership rules, data-structure-aware query planning, predictable recovery, and a presentation layer that makes low-level decisions easy to inspect.</p>'
                        + '<div class="note">Three-minute pitch: pure C storage core, hash + B+ Tree indexing, WAL-backed recovery, shared executor boundary, and a desktop studio that visualizes query plans and runtime metrics.</div>',
                    )
                    + "</div>"
                ),
            },
        ],
    },
    "ko": {
        "html_lang": "ko",
        "body_class": "lang-ko",
        "document_title": "MiniDB Studio 알고리즘 백서 (한국어)",
        "section_label": "섹션",
        "cover_eyebrow": "Algorithm Whitepaper",
        "cover_title": "MiniDB Studio",
        "cover_subtitle": "순수 C 코어, 규칙 기반 query planning, B+ Tree traversal, WAL recovery, 그리고 상태 기반 desktop observability를 정리한 이중 언어 스토리지 엔진 백서.",
        "cover_copy": "이 문서는 현재 코드베이스에 구현된 MiniDB Studio의 실제 구조를 정리한다. 재사용 가능한 engine module, index-aware query execution, deterministic assistant suggestion, 그리고 동일한 executor boundary 위에서 동작하는 raylib desktop synchronization을 그대로 설명한다.",
        "cover_chips": ["Pure C11 Core", "Hash Index", "B+ Tree", "CSV Snapshot", "WAL Replay", "raylib Desktop"],
        "cover_metrics": [("엔진 경계", "재사용 가능"), ("인덱스 경로", "3"), ("복구 모델", "CSV + WAL"), ("문서 세트", "영문 + 국문")],
        "cover_meta": [
            ("대상 독자", "엔지니어링 포트폴리오 검토자, 면접관, 시스템 프로그래밍 동료."),
            ("문서 범위", "parser, planner, execution engine, recovery, desktop synchronization, performance telemetry."),
            ("포지셔닝", "설명 가능한 내부 구조와 데모 친화적 UI를 갖춘 lightweight database management studio."),
        ],
        "cover_visual": {
            "aria": "MiniDB Studio 커버 아키텍처 스택",
            "shell": "Desktop Studio Shell",
            "shell_sub": ["raylib panel", "query editor", "result grid"],
            "planner": "Shared Executor Boundary",
            "planner_sub": ["parser", "planner", "CRUD dispatch"],
            "hash": "Hash + WAL",
            "hash_sub": ["exact match", "append log"],
            "bptree": "B+ Tree + Metrics",
            "bptree_sub": ["range scan", "ordered traversal"],
            "footer": "하나의 code path 위에 terminal과 desktop UI가 함께 올라간다.",
        },
        "cover_card_title": "아키텍처 스냅샷",
        "cover_card_copy": "공유 execution core 덕분에 terminal과 desktop surface 모두에서 query semantic, recovery rule, telemetry가 동일하게 유지된다.",
        "sections": [
            {
                "title": "Project Overview",
                "lead": "MiniDB Studio는 in-memory record engine, rule-based optimizer, 그리고 desktop control surface를 하나의 일관된 구조로 묶는다. 단순 parser 데모가 아니라 작은 database studio처럼 동작하도록 설계된 포트폴리오 프로젝트다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "시스템 범위",
                        unordered(
                            [
                                "<span class=\"code-inline\">C11</span> 기반 storage core가 record, index, recovery state, execution statistic의 수명을 직접 관리한다.",
                                "터미널 콘솔과 raylib desktop app은 동일한 <span class=\"code-inline\">parse -> plan -> execute</span> 경계를 재사용한다.",
                                "정확 일치 조회는 hash index로 가속하고, ordered access와 range scan은 B+ Tree가 담당한다.",
                                "Persistence는 snapshot + log replay 모델로 구성해, 복구 흐름을 짧은 면접 설명 안에서도 명확하게 전달할 수 있다.",
                            ]
                        ),
                    )
                    + card(
                        "모듈 구성",
                        table(
                            ["모듈", "주요 책임", "대표 symbol"],
                            [
                                ["<span class=\"mono\">parser.c</span>", "tokenization, validation, command decoding.", "<span class=\"mono\">parse_select</span>, <span class=\"mono\">parse_update</span>"],
                                ["<span class=\"mono\">planner.c</span>", "규칙 기반 access-path 선택과 sort pushdown 판단.", "<span class=\"mono\">planner_choose_plan</span>"],
                                ["<span class=\"mono\">database.c</span>", "CRUD dispatch, filter evaluation, index maintenance, query statistic 집계.", "<span class=\"mono\">database_select</span>, <span class=\"mono\">database_estimate_memory_usage</span>"],
                                ["<span class=\"mono\">bptree.c</span>", "ordered indexing, split/rebalance logic, range traversal.", "<span class=\"mono\">bptree_insert</span>, <span class=\"mono\">bptree_collect_stats</span>"],
                                ["<span class=\"mono\">wal.c</span>", "append-only mutation log와 restart replay.", "<span class=\"mono\">wal_append_insert</span>, <span class=\"mono\">wal_replay</span>"],
                                ["<span class=\"mono\">studio_app.c</span>", "desktop state transition, result grid behavior, metric streaming.", "<span class=\"mono\">studio_execute_text</span>, <span class=\"mono\">studio_push_metrics</span>"],
                            ],
                        ),
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Core Storage Engine Architecture",
                "lead": "가장 중요한 설계 결정은 ownership 분리다. Record의 생명주기는 <span class=\"code-inline\">Database</span> 안에 고정되고, 보조 index는 <span class=\"code-inline\">Record*</span> 참조만 빌려 쓴다. UI에 전달되는 summary는 별도의 임시 result row를 materialize하여 desktop layer가 storage memory의 owner가 되지 않도록 유지한다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "실행 계층",
                        '<div class="figure"><div class="diagram-frame">'
                        + architecture_svg(
                            {
                                "aria": "핵심 아키텍처 계층",
                                "ui": "Desktop + Terminal",
                                "ui_sub": ["raylib Studio", "CLI shell"],
                                "app": "AppContext",
                                "app_sub": ["history", "logger", "cache"],
                                "parse": "Parser + Planner",
                                "parse_sub": ["tokenize", "predicate decode", "access path"],
                                "db": "Database",
                                "db_sub": ["records", "filters", "stats"],
                                "hash": "Hash Index",
                                "hash_sub": ["id", "department"],
                                "bptree": "Ordered Index Layer",
                                "bptree_sub": ["B+ Tree(id)", "B+ Tree(age)", "range scan", "ORDER BY age"],
                                "csv": "CSV Snapshot",
                                "csv_sub": ["SAVE", "LOAD"],
                                "wal": "WAL",
                                "wal_sub": ["INSERT", "UPDATE", "DELETE"],
                                "boundary": "Shared engine boundary",
                                "boundary_sub": "UI는 caller로만 남고, mutation과 ownership 규칙은 엔진이 책임진다.",
                            }
                        )
                        + '</div><p class="figure-caption">기존 terminal과 desktop studio가 동일한 execution path를 공유한다. 이 분리 덕분에 pure C engine은 유지하고 presentation layer만 확장할 수 있다.</p></div>',
                    )
                    + card(
                        "Memory Ownership Model",
                        '<div class="figure"><div class="diagram-frame">'
                        + ownership_svg(
                            {
                                "aria": "메모리 ownership 모델",
                                "database": "Database",
                                "database_sub": ["record lifetime owner", "exit 시 index 정리"],
                                "records": "Record nodes",
                                "records_sub": ["id, name, age, department", "single source of truth"],
                                "hash": "Hash buckets",
                                "hash_sub": ["Record* 참조만 보유", "record free 금지"],
                                "bptree": "B+ Tree value lists",
                                "bptree_sub": ["Record* 참조만 보유", "leaf fan-out 유지"],
                                "summary": "CommandExecutionSummary",
                                "summary_sub": ["temporary query result", "materialization owner"],
                                "result": "QueryResult rows[]",
                                "result_sub": ["UI-safe copy", "cache 가능한 payload"],
                                "ui": "Studio UI",
                                "ui_sub": ["read-only consumer", "selection + sort state"],
                                "borrow": "점선 화살표 = borrowed reference",
                                "own": "실선 화살표 = owning lifetime",
                            }
                        )
                        + '</div><p class="figure-caption">이 ownership 분리 덕분에 LOAD도 안전하다. 임시 <span class="code-inline">Database</span>를 먼저 완성하고 검증한 뒤에만 <span class="code-inline">AppContext</span>에 교체할 수 있다.</p></div>'
                        + '<div class="note">설계 invariant: mutation path가 index와 persistence metadata를 먼저 갱신하고, 그 다음에 UI가 새로운 상태를 읽는다. raylib layer는 storage object를 직접 free하지 않는다.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Query Planner & Optimizer Algorithm",
                "lead": "Optimizer는 의도적으로 lightweight하고 explainable하다. cost model 대신 query shape에 따라 최적 access path를 고르는 deterministic rule set을 사용하며, 선택된 plan 문자열은 그대로 desktop inspector에 표시된다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Planner Decision Flow",
                        '<div class="figure"><div class="diagram-frame">'
                        + planner_svg(
                            {
                                "aria": "planner 흐름도",
                                "start": "Parsed SELECT",
                                "start_sub": ["predicate", "ORDER BY", "LIMIT"],
                                "exact": "Exact indexed predicate?",
                                "exact_sub": ["id = x", "department = x", "single-key equality"],
                                "hash": "Hash Exact Lookup",
                                "hash_sub": ["가장 낮은 지연", "exact match path"],
                                "range": "Range or ordered age path?",
                                "range_sub": ["age > x", "age >= x", "ORDER BY age LIMIT k"],
                                "bptree": "B+ Tree Range Scan",
                                "bptree_sub": ["ordered leaf walk", "limit pushdown"],
                                "scan": "Full Scan",
                                "scan_sub": ["predicate filter", "필요 시 in-memory sort"],
                                "yes": "yes",
                                "no": "no",
                                "or_path": "또는 OR-path fallback filtering 필요",
                                "fallback": "적절한 index가 없으면 fallback",
                            }
                        )
                        + '</div><p class="figure-caption">Planner는 <span class="code-inline">B+ Tree Range Scan</span>, <span class="code-inline">Hash Exact Lookup</span> 같은 plan label을 생성하고, 이 문자열이 그대로 desktop inspector에 노출된다.</p></div>',
                    )
                    + card(
                        "규칙 집합과 Planner 효과",
                        table(
                            ["Query shape", "Primary plan", "Planner effect"],
                            [
                                ["<span class=\"mono\">SELECT WHERE id = 42</span>", "Hash exact lookup 또는 B+ exact lookup", "하나의 bucket 또는 key slot만 검사하므로 row scan 수가 매우 작다."],
                                ["<span class=\"mono\">SELECT WHERE department = Oncology</span>", "Hash exact lookup", "department hash index를 seed path로 사용하고 후보 chain만 추가 평가한다."],
                                ["<span class=\"mono\">SELECT WHERE age &gt; 30</span>", "B+ Tree range scan", "첫 qualifying leaf에서 시작해 linked leaf chain을 따라간다."],
                                ["<span class=\"mono\">SELECT ORDER BY age LIMIT 10</span>", "B+ Tree ordered scan", "leaf traversal 결과가 이미 정렬되어 있으면 추가 sort를 생략한다."],
                                ["혼합 <span class=\"mono\">AND</span> predicate", "Indexed seed + post-filter", "가장 좋은 seed path를 선택한 뒤 나머지 조건을 in-memory로 평가한다."],
                                ["광범위한 <span class=\"mono\">OR</span> 또는 미지원 조합", "Full scan", "정확성을 우선해 전체 record walk로 fallback한다."],
                            ],
                        )
                        + '<div class="note">Planner는 추가 in-memory sort가 필요한지도 함께 결정한다. leaf traversal이 이미 <span class="code-inline">ORDER BY age</span>를 만족하면 reorder 비용을 피할 수 있다.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "B+ Tree Traversal Logic",
                "lead": "Ordered index layer의 목적은 age 기반 range scan과 ordered traversal을 단순 선형 검색이 아니라 storage-engine 수준의 동작으로 보이게 만드는 것이다. 실제 record는 모두 leaf에 유지하고, internal node는 separator key만 보관하며, leaf는 forward chain으로 연결된다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Split and Range Path",
                        '<div class="figure"><div class="diagram-frame">'
                        + bptree_svg(
                            {
                                "aria": "B+ Tree split과 range traversal",
                                "root": "Internal node",
                                "root_sub": ["separator key로 routing", "record는 leaf에 유지"],
                                "leaf_left": "Leaf A",
                                "leaf_left_sub": ["22, 24, 27", "split 이전"],
                                "leaf_mid": "Leaf B",
                                "leaf_mid_sub": ["31, 34", "첫 qualifying leaf"],
                                "leaf_right": "Leaf C",
                                "leaf_right_sub": ["38, 42, 47", "next 포인터로 연결"],
                                "range": "Range scan",
                                "range_sub": ["age >= 31 위치 탐색", "leaf->next 순회"],
                                "split": "임시 m+1 배열",
                                "promote": "right leaf의 첫 key를 promote",
                                "chain": "Leaf chain이 ordered traversal을 지원",
                            }
                        )
                        + '</div><p class="figure-caption">현재 구현은 <span class="code-inline">bptree.c</span> 안에 split comment를 직접 남겨 leaf/internal promotion 규칙이 코드 리뷰에서도 바로 보이도록 했다.</p></div>',
                    )
                    + card(
                        "Operational Semantics",
                        unordered(
                            [
                                "<span class=\"code-inline\">bptree_insert</span>는 올바른 leaf까지 내려간 뒤 정렬 순서를 유지하며 삽입하고, overflow가 발생했을 때만 split 결과를 상위로 전파한다.",
                                "Leaf split은 여유 슬롯이 하나 있는 임시 배열을 사용해 lower half는 현재 node에 남기고 upper half는 새로운 right sibling으로 이동한 뒤, 그 right sibling의 첫 key를 promote한다.",
                                "Internal split은 가운데 separator를 상위로 promote하면서 child 배열을 재배치해 parent routing을 유지한다.",
                                "중복 age는 하나의 leaf key 아래 value list로 연결되므로, duplicate key를 위해 separator를 반복 생성하지 않아도 된다.",
                                "Delete는 borrow-or-merge rebalancing helper를 사용해 underflow node를 복구한 뒤 caller로 돌아간다.",
                            ]
                        )
                        + table(
                            ["Operation", "Typical complexity", "의미"],
                            [
                                ["Exact lookup", "<span class=\"mono\">O(log_B n)</span>", "separator를 따라 하나의 leaf로 내려간다."],
                                ["Range scan", "<span class=\"mono\">O(log_B n + k)</span>", "한 번 seek하고 leaf chain을 스트리밍한다."],
                                ["Insert", "<span class=\"mono\">O(log_B n)</span>", "overflow node에서만 split 발생."],
                                ["Delete", "<span class=\"mono\">O(log_B n)</span>", "underflow 시 borrow/merge."],
                            ],
                            mono_cols={1},
                        ),
                    )
                    + "</div>"
                ),
            },
            {
                "title": "WAL and Snapshot Recovery Flow",
                "lead": "Persistence는 checkpoint 가능한 snapshot과 append-only mutation log 조합으로 모델링된다. 이 방식은 디스크 동작을 이해하기 쉽다. SAVE는 안정적인 CSV 이미지를 만들고, LOAD는 그 이후 mutation을 WAL에서 replay한다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Recovery Sequence",
                        '<div class="figure"><div class="diagram-frame">'
                        + recovery_svg(
                            {
                                "aria": "snapshot과 WAL recovery 흐름",
                                "load": "LOAD command",
                                "load_sub": ["임시 database 생성"],
                                "csv": "CSV snapshot",
                                "csv_sub": ["persistence_load_csv"],
                                "wal": "WAL replay",
                                "wal_sub": ["wal_replay", "INSERT | UPDATE | DELETE"],
                                "swap": "Live state 교체",
                                "swap_sub": ["AppContext database 교체", "status + memory 갱신"],
                                "save": "SAVE command",
                                "save_sub": ["persistence_save_csv"],
                                "checkpoint": "WAL checkpoint",
                                "checkpoint_sub": ["durable snapshot 후 log 정리"],
                                "isolation": "임시 restore가 끝까지 성공했을 때만 live state를 교체한다.",
                            }
                        )
                        + '</div><p class="figure-caption">Executor는 먼저 임시 <span class="code-inline">Database</span>에 CSV를 로드하고 <span class="code-inline">data/db.log</span>를 replay한 뒤에만 active context를 교체한다.</p></div>',
                    )
                    + card(
                        "Recovery Guarantees",
                        table(
                            ["단계", "현재 알고리즘", "의미"],
                            [
                                ["Mutation logging", "<span class=\"mono\">INSERT|...</span>, <span class=\"mono\">UPDATE|...</span>, <span class=\"mono\">DELETE|...</span> 레코드를 append.", "마지막 snapshot 이후 모든 변경이 재생 가능한 형태로 남는다."],
                                ["Snapshot save", "모든 row를 CSV로 기록한 뒤 WAL을 checkpoint.", "CSV가 간결한 restore point가 되고 log 길이도 제어된다."],
                                ["Restore", "임시 engine에 CSV를 load하고 WAL을 replay하며 record insert 과정에서 index를 함께 복원.", "정상 실행과 동일한 code path로 index 일관성이 복원된다."],
                                ["Activation", "성공 후에만 restored database를 <span class=\"mono\">AppContext</span>에 대입.", "부분 복구 상태가 active session을 오염시키지 않는다."],
                            ],
                        )
                        + '<div class="note">Replay가 normal database mutation API를 그대로 호출하기 때문에, index consistency는 별도의 recovery 단계가 아니라 insert/update/delete 재사용의 결과로 자연스럽게 유지된다.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "AI Query Assistant Parsing Flow",
                "lead": "현재 desktop assistant는 의도적으로 deterministic하다. LLM runtime이 아니라, row context와 이전 execution plan, 그리고 demo heuristic을 바탕으로 유효한 SQL-like suggestion을 생성한 뒤, 그 suggestion을 사용자가 입력한 text와 동일한 parser 경로로 다시 흘려보낸다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Current Assistant Pipeline",
                        '<div class="figure"><div class="diagram-frame">'
                        + assistant_svg(
                            {
                                "aria": "assistant suggestion 흐름",
                                "row": "Selected row",
                                "row_sub": ["id, department, age context"],
                                "plan": "Last plan feedback",
                                "plan_sub": ["full scan 경고", "index-aware hint"],
                                "assistant": "studio_build_assistant_content",
                                "assistant_sub": ["유효한 query template 생성", "engine grammar 유지"],
                                "editor": "SQL editor",
                                "editor_sub": ["suggestion 삽입", "Ctrl+Enter 실행"],
                                "parser": "Shared parser",
                                "parser_sub": ["동일한 parse/plan path"],
                                "shared": "추천 command도 특별 취급 없이 동일한 execution path를 통과한다.",
                                "note": "이 설계는 assistant를 정직하게 만든다. 모든 suggestion은 manual query와 같은 parser contract를 만족해야 한다.",
                            }
                        )
                        + '</div><p class="figure-caption">예를 들어 row 선택이나 plan feedback을 바탕으로 <span class="code-inline">SELECT WHERE id = ...</span>, <span class="code-inline">SELECT WHERE age &gt; ... ORDER BY age LIMIT 10</span> 같은 query를 제안할 수 있다.</p></div>',
                    )
                    + card(
                        "Assistant Design Contract",
                        unordered(
                            [
                                "선택된 row 정보가 deterministic query template의 seed가 되므로, 현재 grammar에서 항상 유효한 문장만 생성한다.",
                                "직전 execution plan이 full scan이었다면 더 index-friendly한 대안을 우선 제안한다.",
                                "Editor가 항상 visible source of truth로 남기 때문에, 사용자는 실행 전에 suggestion을 직접 확인하고 수정할 수 있다.",
                                "Assistant output도 <span class=\"code-inline\">parse_command</span>를 다시 통과하므로 validation, logging, benchmarking, caching이 그대로 유지된다.",
                            ]
                        )
                        + '<div class="note">실제 natural-language parsing은 roadmap에 두는 것이 맞다. 현재 구현은 engine state를 context-aware query suggestion으로 바꿔 주는 형태의 “AI assistant”를 제공한다.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Desktop UI-State Synchronization",
                "lead": "Desktop application이 반응성을 유지하는 이유는 UI state가 얇고 파생적이기 때문이다. Query execution은 <span class=\"code-inline\">AppContext</span> 내부에서 engine state를 갱신하고, desktop layer는 editor, grid, inspector, metric dashboard, footer에 필요한 summary만 소비한다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "State Transition Graph",
                        '<div class="figure"><div class="diagram-frame">'
                        + sync_svg(
                            {
                                "aria": "desktop UI synchronization 흐름",
                                "action": "User action",
                                "action_sub": ["type, click, shortcut"],
                                "execute": "studio_execute_text",
                                "execute_sub": ["run input", "selection state 초기화"],
                                "engine": "app_context_run_input",
                                "engine_sub": ["parse", "plan", "execute", "log"],
                                "summary": "CommandExecutionSummary",
                                "summary_sub": ["message", "result rows", "stats", "plan"],
                                "grid": "Result grid",
                                "grid_sub": ["rows, sort, page"],
                                "metrics": "Metric buffers",
                                "metrics_sub": ["latency, scans, memory"],
                                "inspector": "Inspector",
                                "inspector_sub": ["plan, index, benchmark"],
                                "status": "Status bar",
                                "status_sub": ["records, storage, queries"],
                            }
                        )
                        + '</div><p class="figure-caption">Desktop shell은 storage view를 직접 재구성하지 않는다. 대신 compact execution summary와 dashboard status snapshot을 받아 필요한 패널만 갱신한다.</p></div>',
                    )
                    + card(
                        "Synchronization Points",
                        table(
                            ["UI event", "주요 함수", "상태 변화"],
                            [
                                ["Query 실행", "<span class=\"mono\">studio_execute_text</span>", "command를 실행하고 최신 summary를 반영하며 오래된 row selection 상태를 정리한다."],
                                ["Engine dispatch", "<span class=\"mono\">app_context_run_input</span>", "cache counter, history, logger, summary payload를 하나의 경계 안에서 갱신한다."],
                                ["Metrics update", "<span class=\"mono\">studio_push_metrics</span>", "latency, rows scanned, memory usage, cache ratio, chosen plan counter를 append한다."],
                                ["Grid sort", "<span class=\"mono\">result_grid_sort</span>", "materialized result row만 재정렬하고 underlying table은 수정하지 않는다."],
                                ["Footer refresh", "<span class=\"mono\">app_context_snapshot_status</span>", "record count, memory estimate, session query total을 갱신한다."],
                            ],
                        )
                        + '<div class="note">이 흐름 덕분에 studio는 시각적으로 풍부해도 별도의 storage layer가 되지 않는다. UI는 presentation state를, engine은 truth를 가진다.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Performance Dashboard Metrics",
                "lead": "MiniDB Studio는 engine 내부 동작을 presentation-friendly telemetry로 드러낸다. 각 command 실행 후 compact metric series를 갱신하므로, desktop dashboard는 결과뿐 아니라 결과가 어떤 비용 구조로 만들어졌는지도 설명할 수 있다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Metric Data Flow",
                        '<div class="figure"><div class="diagram-frame">'
                        + metrics_svg(
                            {
                                "aria": "performance metric sparkline과 usage chart",
                                "latency": "Latency trend",
                                "usage": "Index usage frequency",
                                "latency_axis": "최근 query 순서",
                                "cache": "Cache",
                                "hash": "Hash",
                                "bptree": "B+",
                                "full": "Scan",
                                "footer": "실행마다 latency, rows scanned, memory, cache ratio, selected-plan counter가 갱신된다.",
                            }
                        )
                        + '</div><p class="figure-caption">Dashboard는 이미 execution summary에 존재하는 engine-side signal을 시각화한다. 즉 UI가 값을 만들어내는 것이 아니라, 엔진이 집계한 telemetry를 읽는다.</p></div>',
                    )
                    + card(
                        "Metric Definitions",
                        table(
                            ["Metric", "Source field", "운영적 의미"],
                            [
                                ["Query latency trend", "<span class=\"mono\">last_elapsed_ms</span>", "가장 최근 command path의 wall-clock duration."],
                                ["Rows scanned", "<span class=\"mono\">summary.stats.rows_scanned</span>", "해당 query를 처리하기 위해 접근한 record 또는 index candidate 수."],
                                ["Memory usage", "<span class=\"mono\">database_estimate_memory_usage</span>", "record, index, B+ Tree value node를 포함한 현재 storage footprint 추정치."],
                                ["Cache hit ratio", "<span class=\"mono\">query_cache_hits / query_cache_lookups</span>", "반복 가능한 command가 session cache로 처리되는 비율."],
                                ["Index usage frequency", "chosen plan 기준 counter", "세션 동안 hash, B+ Tree, cache, full-scan path가 얼마나 선택되었는지."],
                            ],
                        )
                        + '<div class="note">Dashboard는 의도적으로 lightweight하다. 작은 chart buffer만 유지하고, 별도 telemetry subsystem 없이 execution accounting 결과를 그대로 보여 준다.</div>',
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Complexity Analysis",
                "lead": "이 엔진은 asymptotic 관점에서 설명하기에 충분히 작고, exact lookup 구조와 ordered traversal, recovery 작업 사이의 trade-off를 보여 주기에 충분히 깊다. 아래 차트는 리뷰어가 기대해야 할 상대적 비용 프로파일을 요약한다.",
                "body": lambda c: (
                    '<div class="grid-2">'
                    + card(
                        "Relative Cost Profile",
                        '<div class="figure"><div class="diagram-frame">'
                        + complexity_svg(
                            {
                                "aria": "상대 복잡도 차트",
                                "title": "Relative operation cost",
                                "rows": [
                                    "Parse and tokenize",
                                    "Hash exact lookup",
                                    "B+ Tree exact lookup",
                                    "B+ Tree range scan",
                                    "Snapshot load + WAL replay",
                                ],
                                "labels": [
                                    "O(L)",
                                    "Avg O(1), worst O(n)",
                                    "O(log_B n)",
                                    "O(log_B n + k)",
                                    "O(n) + O(m log_B n)",
                                ],
                            }
                        )
                        + '</div><p class="figure-caption">정렬이 중요한 query에서는 point lookup보다 B+ Tree range scan이 더 좋은 path가 될 수 있다. 비용 구조가 다르기 때문이다.</p></div>',
                    )
                    + card(
                        "Complexity Table",
                        table(
                            ["Operation", "Complexity", "의미"],
                            [
                                ["Tokenize + parse", "<span class=\"mono\">O(L)</span>", "query text 길이에 선형이므로 front end 동작이 예측 가능하다."],
                                ["Hash exact lookup", "<span class=\"mono\">O(1)</span> average", "id 또는 department equality filter에서 가장 강력하다."],
                                ["B+ Tree exact lookup", "<span class=\"mono\">O(log_B n)</span>", "작은 tree height로 안정적인 ordered search를 제공한다."],
                                ["B+ Tree range scan", "<span class=\"mono\">O(log_B n + k)</span>", "한 번 seek한 뒤 leaf chain을 streaming한다."],
                                ["Insert / delete with index maintenance", "<span class=\"mono\">O(log_B n)</span>", "record lookup/allocation 이후에는 B+ Tree maintenance가 지배 비용이다."],
                                ["Snapshot load", "<span class=\"mono\">O(n)</span>", "모든 row가 normal insert path로 다시 구성된다."],
                                ["WAL replay", "<span class=\"mono\">O(m log_B n)</span>", "replay 길이와 index rebuild 비용에 비례한다."],
                                ["Result-grid sort fallback", "<span class=\"mono\">O(r log r)</span>", "planner가 native order를 보존하지 못할 때만 사용한다."],
                            ],
                            mono_cols={1},
                        ),
                    )
                    + "</div>"
                ),
            },
            {
                "title": "Future Scaling Roadmap",
                "lead": "현재 구조는 의도적으로 compact하지만, 이미 더 큰 storage-engine 작업으로 확장될 seam을 갖고 있다. 아래 roadmap은 pure C core를 유지하면서 disk management, richer planning, 그리고 더 강한 assistant behavior로 확장하는 방향을 정리한다.",
                "body": lambda c: (
                    '<div class="roadmap">'
                    + "".join(
                        [
                            '<article class="horizon"><div class="small-label">Near-term</div><h3 class="horizon-title">Local engine depth</h3><p>slotted-page serialization, richer index statistic, 더 정교한 predicate normalization을 추가하되 현재 in-memory API surface는 유지한다.</p></article>',
                            '<article class="horizon"><div class="small-label">Mid-term</div><h3 class="horizon-title">Storage realism</h3><p>buffer-pool abstraction, asynchronous checkpointing, composite index, 더 명시적인 recovery metadata를 도입해 SAVE/LOAD를 page-oriented persistence로 확장한다.</p></article>',
                            '<article class="horizon"><div class="small-label">Long-term</div><h3 class="horizon-title">DB studio evolution</h3><p>기존 parser boundary를 유지한 채 MVCC, cost-based optimizer, validated natural-language-to-query conversion을 올린다.</p></article>',
                        ]
                    )
                    + '<div class="card quote-card"><h3 class="card-title">Roadmap Principle</h3><p>다음 단계로 갈수록 현재 프로젝트의 장점은 더 강화되어야 한다. explainable internals, reusable boundary, 그리고 실제 engine behavior를 시각화하는 UI가 핵심이다.</p></div>'
                ),
            },
            {
                "title": "Final Engineering Summary",
                "lead": "MiniDB Studio가 포트폴리오 시스템으로 설득력을 가지는 이유는 engine, optimizer, recovery path, desktop shell이 하나의 일관된 서사를 만들기 때문이다. 몇 분 안에 설명할 수 있을 만큼 compact하고, indexing, ownership, execution trade-off를 깊게 이야기할 만큼 충분히 rich하다.",
                "body": lambda c: (
                    '<div class="footer-summary">'
                    + card(
                        "왜 이 설계가 일관적인가",
                        '<div class="timeline">'
                        + "".join(
                            [
                                '<div class="timeline-row"><div class="timeline-tag">Reusable core</div><div>동일한 parser, planner, database code가 console과 desktop studio를 함께 구동한다. 그래서 구조가 “복제된 UI”가 아니라 의도된 architecture로 보인다.</div></div>',
                                '<div class="timeline-row"><div class="timeline-tag">Explainable performance</div><div>Hash lookup, B+ Tree traversal, scan count, cache hit가 모두 눈에 보이므로 하나의 query path를 실제 엔진 수준에서 설명할 수 있다.</div></div>',
                                '<div class="timeline-row"><div class="timeline-tag">Recovery clarity</div><div>CSV snapshot + WAL replay 조합은 durability, restore safety, index reconstruction을 명확한 mental model로 정리해 준다.</div></div>',
                            ]
                        )
                        + "</div>",
                    )
                    + card(
                        "포트폴리오 포지셔닝",
                        '<p class="card-copy">MiniDB Studio는 시스템 프로그래밍 면접에서 좋은 인상을 주는 판단을 보여 준다. explicit ownership rule, data-structure-aware query planning, predictable recovery, 그리고 low-level decision을 읽기 쉽게 드러내는 presentation layer가 핵심이다.</p>'
                        + '<div class="note">20초 데모 포인트: pure C storage core, hash + B+ Tree indexing, WAL-backed recovery, shared executor boundary, 그리고 query plan과 runtime metric을 시각화하는 desktop studio.</div>',
                    )
                    + "</div>"
                ),
            },
        ],
    },
}


def render_document(language: str) -> str:
    copy = COPY[language]

    cover_metrics_html = "".join(metric(label, value) for label, value in copy["cover_metrics"])
    meta_html = "".join(meta_card(label, value) for label, value in copy["cover_meta"])
    chips_html = "".join(f'<span class="chip">{html.escape(chip)}</span>' for chip in copy["cover_chips"])

    sections_html = []
    for index, section in enumerate(copy["sections"], start=2):
        sections_html.append(
            wrap_section(
                index,
                copy["section_label"],
                section["title"],
                section["lead"],
                section["body"](copy),
                break_before=True,
            )
        )

    title_page = dedent(
        f"""
        <section class="page cover">
            <div class="cover-grid">
                <div class="hero-block">
                    <div class="eyebrow">{html.escape(copy["cover_eyebrow"])}</div>
                    <h1 class="title">{html.escape(copy["cover_title"])}</h1>
                    <p class="subtitle">{copy["cover_subtitle"]}</p>
                    <p class="hero-copy">{copy["cover_copy"]}</p>
                    <div class="chips">{chips_html}</div>
                    <div class="hero-metrics">{cover_metrics_html}</div>
                </div>
                <div class="cover-visual">
                    <div class="cover-card">
                        <div class="cover-card-title">{copy["cover_card_title"]}</div>
                        <div class="cover-card-copy">{copy["cover_card_copy"]}</div>
                    </div>
                    <div class="diagram-frame">
                        {cover_visual_svg(copy["cover_visual"])}
                    </div>
                </div>
            </div>
            <div>
                <div class="meta-grid">{meta_html}</div>
            </div>
        </section>
        """
    ).strip()

    html_document = dedent(
        f"""
        <!DOCTYPE html>
        <html lang="{copy["html_lang"]}">
        <head>
            <meta charset="utf-8">
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <title>{html.escape(copy["document_title"])}</title>
            <style>
            {BASE_CSS}
            </style>
        </head>
        <body class="{copy["body_class"]}">
            <main class="paper">
                {title_page}
                {''.join(sections_html)}
            </main>
        </body>
        </html>
        """
    ).strip()

    return html_document


def export_with_browser(browser: Path, input_html: Path, output_pdf: Path, output_png: Path) -> None:
    file_url = input_html.resolve().as_uri()

    pdf_command = [
        str(browser),
        "--headless=new",
        "--disable-gpu",
        "--run-all-compositor-stages-before-draw",
        "--virtual-time-budget=4000",
        "--print-to-pdf-no-header",
        f"--print-to-pdf={output_pdf.resolve()}",
        file_url,
    ]
    subprocess.run(pdf_command, check=True)

    png_command = [
        str(browser),
        "--headless=new",
        "--disable-gpu",
        "--run-all-compositor-stages-before-draw",
        "--virtual-time-budget=4000",
        "--hide-scrollbars",
        "--window-size=1600,1100",
        "--force-device-scale-factor=2",
        f"--screenshot={output_png.resolve()}",
        file_url,
    ]
    subprocess.run(png_command, check=True)


def main() -> None:
    DOCS_DIR.mkdir(parents=True, exist_ok=True)

    browser = find_chrome()
    for language, paths in OUTPUTS.items():
        html_output = render_document(language)
        paths["html"].write_text(html_output, encoding="utf-8")
        export_with_browser(browser, paths["html"], paths["pdf"], paths["png"])

    for language, paths in OUTPUTS.items():
        print(f"[{language}]")
        for kind, path in paths.items():
            size_kb = path.stat().st_size / 1024
            print(f"  {kind}: {path.name} ({size_kb:.1f} KB)")


if __name__ == "__main__":
    main()
