"""Synchronise the actual recorded imagegen prompts into the repository source document."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT/'ArtSource/StorybookV1/V083'
TARGET = ROOT/'docs/54-v0.8.3Assets.md'
MARKER = '## 完整图片提示词'

def main():
    rows = json.loads((SRC/'generation.json').read_text(encoding='utf-8-sig'))
    if isinstance(rows, dict):
        rows = rows.get('items', [])
    assert len(rows) == 14, 'Record one actual generation per five role atlases and nine spell cards'
    text = TARGET.read_text(encoding='utf-8')
    assert MARKER in text
    appendix = [MARKER, '', '以下为本次实际传入内置 `imagegen` 的完整提示词。原始输出按字节归档；SHA-256 用于确认对应文件。工具未提供可核验的随机种子或模型版本，本页不补造。', '']
    for row in rows:
        path = ROOT/row['file']
        if not path.is_file():
            path = SRC/row['file']
        assert path.is_file() and row.get('prompt'), row
        key = row.get('id', path.stem)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        appendix.extend(['### '+str(key), '',
            '- 原图：['+path.name+'](../'+path.relative_to(ROOT).as_posix()+')。',
            '- SHA-256：`'+digest+'`。',
            '- 工具：`'+str(row.get('tool', 'built-in imagegen'))+'`。',
            '- 实际返回路径与生成日期：见 `generation.json` 对应条目。', '',
            '```text', row['prompt'], '```', ''])
    TARGET.write_text(text.split(MARKER, 1)[0]+'\n'.join(appendix).rstrip()+'\n', encoding='utf-8')
    print('V083_PROMPTS_DOCUMENTED', len(rows))

if __name__ == '__main__':
    main()
