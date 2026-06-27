---
name: os-lab-skill
description: Draft, revise, polish, or review Chinese operating-system lab reports in Markdown. Use when working on OS lab report sections, thought questions, result analysis, troubleshooting notes, or references involving Linux kernel/QEMU/GDB, MBR and real mode, protected mode/GDT, mixed C/C++ and assembly kernels, interrupts, kernel threads and scheduling, synchronization, deadlock, monitors, paging, address pools, and memory management.
---

# OS Lab Skill

## Core Workflow

Use this skill to produce Chinese Markdown that follows the user's existing OS lab report style: concrete experiment steps, concise implementation analysis, evidence-backed results, and targeted answers to thought questions.

1. Identify whether the user wants drafting, revision, polishing, review, troubleshooting, or reference help.
2. Read `references/report-style.md` when matching the report format, tone, and section structure matters.
3. Read `references/os-lab-reference.md` when explaining OS concepts, commands, formulas, or lab-specific mechanisms.
4. Read `references/troubleshooting.md` when writing "遇到的问题及解决方法" sections or diagnosing build/QEMU/GDB issues.
5. Use the user's provided code, screenshots, logs, and assignment requirements as the source of truth.
6. Write in Chinese unless the user explicitly requests another language.

## Evidence Rules

- Do not invent screenshots, terminal output, GDB output, addresses, or benchmark numbers.
- If evidence is missing, insert a clear placeholder such as `![运行截图](待补充截图路径)` or `> 运行结果待补充：此处应放置 QEMU 输出截图。`
- Separate "实验现象", "实现思路", "结果分析", and "原因解释"; do not blur observed facts with expected behavior.
- Preserve or add an AI-assistance disclosure when substantial code or explanation is generated with model help, matching the existing report style.

## Report Writing Pattern

For each Assignment or Part, prefer this compact order:

1. Briefly state the task goal and environment if it changes.
2. Show key commands or core code only; omit long unchanged boilerplate unless requested.
3. Insert screenshot or terminal-output evidence.
4. Analyze the mechanism behind the result, using registers, memory layout, scheduler state, locks, or page-table relationships when relevant.
5. Answer thought questions directly, with formulas or step-by-step reasoning where useful.
6. Add "遇到的问题及解决方法" only for real issues the user encountered.

## Output Standards

- Keep headings in the existing style: `# <center>LabX 标题</center>`, `## AssignmentN ...`, `### N.M ...`, or `## PartN ...` depending on the lab.
- Use fenced code blocks with language tags when obvious, for example `bash`, `asm`, `cpp`, or `text`.
- Use bold labels for short analysis blocks: `**实现思路：**`, `**结果分析：**`, `**原因：**`, `**解决：**`.
- Prefer concise technical explanations over generic praise or long summaries.
- For review tasks, lead with factual issues: missing evidence, unclear causal analysis, incorrect formula, unsupported result, or formatting inconsistency.
