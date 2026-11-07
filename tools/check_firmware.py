from pathlib import Path
import xml.etree.ElementTree as ET
p = Path(__file__).resolve().parents[1]/'firmware/MDK-ARM/DJI C board.uvprojx'
t = ET.parse(p)
missing = []
for tag in ('FilePath', 'IncludePath'):
    for node in t.iter(tag):
        for value in (node.text or '').split(';'):
            if value and not (p.parent/value).exists(): missing.append((tag, value))
print('Device:', t.findtext('.//Device'))
print('Compiler:', t.findtext('.//pCCUsed'))
print('Pack:', t.findtext('.//PackID'))
print('Source entries:', len(list(t.iter('FilePath'))))
print('Missing references:', missing)
raise SystemExit(bool(missing))
