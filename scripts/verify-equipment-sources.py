#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Manual research verification, never called by either application.
Verify pinned Surrey IR magnitude ratios and Keiang numerical graph readings against
source bytes. --fetch permits bounded HTTPS downloads; otherwise only cached data is
read. Third-party WAVs/plots stay in .cache, not in the shipped catalog.
Requires NumPy, SciPy and Pillow for development only.
"""
import argparse,hashlib,io,json,urllib.request,zipfile
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from scipy.fft import rfft,rfftfreq
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
SIZE=1485159533
URL='https://zenodo.org/api/records/4633508/files/Microphone_Impulse_Responses.zip/content'
class RemoteZip(io.RawIOBase):
 def __init__(self,cache,fetch):self.pos=0;self.cache=cache;self.fetch=fetch
 def seekable(self):return True
 def readable(self):return True
 def tell(self):return self.pos
 def seek(self,n,whence=0):
  self.pos=n if whence==0 else self.pos+n if whence==1 else SIZE+n
  if not 0<=self.pos<=SIZE:raise ValueError('Out-of-bounds archive seek')
  return self.pos
 def read(self,n=-1):
  if n<0:n=SIZE-self.pos
  n=min(n,SIZE-self.pos)
  if not n:return b''
  if not 0<n<=16*1024*1024:raise ValueError('Archive range over 16 MiB')
  dest=self.cache/f'{self.pos}-{n}.bin'
  if dest.exists():b=dest.read_bytes()
  else:
   if not self.fetch:raise RuntimeError('Missing cached archive range; use --fetch explicitly')
   req=urllib.request.Request(URL,headers={'Range':f'bytes={self.pos}-{self.pos+n-1}'})
   with urllib.request.urlopen(req,timeout=45) as r:
    if r.status!=206 or r.headers.get('Content-Range')!=f'bytes {self.pos}-{self.pos+n-1}/{SIZE}':raise ValueError('Unexpected range response')
    b=r.read(n+1)
   if len(b)!=n:raise ValueError('Truncated archive range')
   dest.write_bytes(b)
  if len(b)!=n:raise ValueError('Corrupt cached range')
  self.pos+=n;return b

def digest(b):return hashlib.sha256(b).hexdigest()
def spectrum(b):
 rate,x=wavfile.read(io.BytesIO(b))
 if rate!=48000 or x.ndim!=1 or len(x)!=480000 or not np.isfinite(x).all() or np.max(abs(x[1500:]))!=0:raise ValueError('Unexpected windowed IR')
 return abs(rfft(x[:1500].astype(float),n=131072))
def compare(actual,expected):
 if np.max(np.abs(np.array(actual)-np.array(expected)))>0.0001:raise ValueError('Numerical extraction differs from pinned input')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fetch',action='store_true');p.add_argument('--surrey',action='store_true');p.add_argument('--amplifiers',action='store_true');args=p.parse_args()
 if not(args.surrey or args.amplifiers):p.error('Choose --surrey and/or --amplifiers')
 report=json.loads((ROOT/'data/equipment/published-collection-report.json').read_text());raw=json.loads((ROOT/'data/equipment/published-response-inputs.json').read_text());audits={a['id']:a for a in report['sourceAudit'] if 'id' in a}
 cache=ROOT/'.cache/equipment-research';cache.mkdir(parents=True,exist_ok=True)
 count=0
 if args.surrey:
  ranges=cache/'surrey';ranges.mkdir(exist_ok=True)
  with zipfile.ZipFile(RemoteZip(ranges,args.fetch)) as archive:
   reference=archive.read('Microphone_Impulse_Responses/Raw_IRs/NTiM2211_125cm/32bit/NTiM2211_125cm_0Deg_32bit.wav');referenceSpectrum=spectrum(reference);frequencies=rfftfreq(131072,1/48000)
   for record in raw:
    if record['measurementSource']!='https://zenodo.org/records/4633508':continue
    a=audits[record['id']]
    if digest(reference)!=a['referenceSha256']:raise ValueError('Reference hash mismatch')
    member=record['provenance'].split('ZIP member ',1)[1].split('; member SHA256',1)[0]
    if not member.startswith('Raw_IRs/') or not member.endswith('_125cm_0Deg_32bit.wav'):raise ValueError('Unexpected member')
    data=archive.read('Microphone_Impulse_Responses/'+member) # ZIP CRC is checked by zipfile.
    if digest(data)!=a['sha256']:raise ValueError('IR source hash mismatch')
    ratio=20*np.log10(np.maximum(spectrum(data),1e-12)/np.maximum(referenceSpectrum,1e-12))
    f=np.geomspace(100,16000,97);v=np.array([np.mean(ratio[(frequencies>=hz/2**(1/6))&(frequencies<=hz*2**(1/6))]) for hz in f]);v-=np.interp(np.log(1000),np.log(f),v)
    compare(np.column_stack((np.round(f,4),np.round(v,5))),record['response']);count+=1
 if args.amplifiers:
  for record in raw:
   a=audits[record['id']]
   if 'traceUrls' not in a:continue
   traces=[]
   for channel,(url,sha,box) in enumerate(zip(a['traceUrls'],a['traceSha256'],a['plotCalibration'])):
    dest=cache/'keiang'/(hashlib.sha256(url.encode()).hexdigest()+'.jpeg');dest.parent.mkdir(exist_ok=True)
    if not dest.exists():
     if not args.fetch:raise RuntimeError('Missing cached source; use --fetch explicitly')
     if not url.startswith('https://www.keiang.de/media/images/Audio_THD_DSCOPE3/amplifier_measurements/'):raise ValueError('Unexpected source URL')
     with urllib.request.urlopen(url,timeout=25) as response:data=response.read(4*1024*1024+1)
     if len(data)>4*1024*1024 or digest(data)!=sha:raise ValueError('Source exceeds limit or hash differs')
     dest.write_bytes(data)
    data=dest.read_bytes()
    if digest(data)!=sha:raise ValueError('Plot source hash differs')
    image=Image.open(io.BytesIO(data))
    if image.width*image.height>32000000:raise ValueError('Plot resolution exceeds bound')
    rgb=np.asarray(image.convert('RGB')).astype(int);left,right,top,bottom=[box[k] for k in ['left','right','top','bottom']]
    if not (0<=left<right<image.width and 0<=top<bottom<image.height):raise ValueError('Invalid graph calibration')
    rgb=rgb[top+2:bottom-1,left+2:right-1]
    mask=(rgb[:,:,2]-rgb[:,:,0]>35)&(rgb[:,:,2]-rgb[:,:,1]>15) if channel==0 else (rgb[:,:,0]-rgb[:,:,2]>35)&(rgb[:,:,0]-rgb[:,:,1]>15)
    xx=[];yy=[]
    for x in range(mask.shape[1]):
     ys=np.where(mask[:,x])[0]
     if len(ys):xx.append(x+left+2);yy.append(np.median(ys)+top+2)
    if len(xx)<.92*(right-left):raise ValueError('Insufficient trace coverage')
    f=10*4000**((np.array(xx)-left)/(right-left));db=((top+bottom)/2-np.array(yy))*10/(bottom-top)
    grid=np.geomspace(20,20000,129);v=np.interp(np.log(grid),np.log(f),db);v-=np.interp(np.log(1000),np.log(grid),v);traces.append(v)
   compare(np.column_stack((np.round(grid,4),np.round((traces[0]+traces[1])/2,5))),record['response']);count+=1
 print(f'PASS: {count} independently recomputed numerical source responses, source hashes and ZIP member CRCs')
if __name__=='__main__':main()
