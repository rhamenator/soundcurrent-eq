# SPDX-License-Identifier: GPL-3.0-only
import argparse,base64,io,zipfile,time,libvirt
from pathlib import Path
parser=argparse.ArgumentParser(description='Receive bounded synthetic audit output from an owned libvirt serial console; no network listener')
parser.add_argument('--vm',required=True)
parser.add_argument('--output',required=True,type=Path)
args=parser.parse_args()
root=args.output;root.mkdir(parents=True,exist_ok=True)
c=libvirt.open('qemu:///system');d=c.lookupByName(args.vm);s=c.newStream();d.openConsole(None,s,libvirt.VIR_DOMAIN_CONSOLE_FORCE)
buf=bytearray();end=b'SOUNDCURRENT_REPORT_END';begin=b'SOUNDCURRENT_REPORT_BEGIN'
try:
 while end not in buf:
  chunk=s.recv(65536)
  if not chunk:raise RuntimeError('Serial stream ended before report')
  buf.extend(chunk)
  if len(buf)>8*1024*1024:raise RuntimeError('Report exceeded limit')
 body=bytes(buf).split(begin,1)[1].split(end,1)[0]
 payload=base64.b64decode(b''.join(body.split()),validate=True)
 z=zipfile.ZipFile(io.BytesIO(payload));members=z.infolist()
 if len(members)>128 or sum(f.file_size for f in members)>16*1024*1024:raise RuntimeError('Uncompressed report exceeded limit')
 if len({f.filename for f in members})!=len(members):raise RuntimeError('Duplicate report member')
 stamp=str(time.time_ns());dest=root/stamp;dest.mkdir()
 for f in z.infolist():
  if Path(f.filename).name!=f.filename or not f.filename.endswith(('.txt','.json','.png')) or f.file_size>4*1024*1024:raise RuntimeError('Unexpected report member')
  data=z.read(f)
  if f.filename.endswith('.png'):
   import struct
   if data[:8]!=b'\x89PNG\r\n\x1a\n' or data[12:16]!=b'IHDR':raise RuntimeError('Invalid screenshot')
   w,h=struct.unpack('>II',data[16:24])
   if not (0<w<=4096 and 0<h<=4096):raise RuntimeError('Screenshot dimensions exceeded limit')
  (dest/f.filename).write_bytes(data)
 (root/(stamp+'.zip')).write_bytes(payload)
 print('Received bounded Windows 10 report:',dest)
finally:
 s.abort();c.close()
