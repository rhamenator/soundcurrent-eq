#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Opt-in integration test: private runtime/server, no host devices.
import subprocess,tempfile,os,time,json,pathlib,sys
binary=str(pathlib.Path(sys.argv[1] if len(sys.argv)>1 else pathlib.Path(__file__).resolve().parents[1]/'build/soundcurrent-eq').resolve())
with tempfile.TemporaryDirectory(prefix='sc-effects-pw-') as directory:
 env=os.environ.copy();env['XDG_RUNTIME_DIR']=directory;env['PIPEWIRE_RUNTIME_DIR']=directory;env['QT_QPA_PLATFORM']='offscreen'
 log=open(directory+'/server.log','w+')
 server=subprocess.Popen(['pipewire'],env=env,stdout=log,stderr=log)
 graph=None
 source=None
 sink=None
 try:
  for i in range(40):
   if pathlib.Path(directory+'/pipewire-0').exists():break
   time.sleep(.1)
  config=subprocess.check_output([binary,'--dump-filter-config','unused-isolated-target'],env=env,text=True)
  pathlib.Path(directory+'/effects.conf').write_text(config)
  graphlog=open(directory+'/graph.log','w+')
  graph=subprocess.Popen(['pipewire','-c',directory+'/effects.conf'],env=env,stdout=graphlog,stderr=graphlog)
  node=None
  for i in range(40):
   time.sleep(.1)
   if graph.poll() is not None:
    graphlog.seek(0);raise RuntimeError(graphlog.read())
   nodes=json.loads(subprocess.check_output(['pw-dump'],env=env))
   for n in nodes:
    if n.get('info',{}).get('props',{}).get('node.name')=='soundcurrent_eq':node=n
   if node:break
  if not node:raise RuntimeError('No isolated EQ node')
  source=subprocess.Popen(['pw-cat','--playback','--raw','--rate','48000','--channels','2','--format','f32','--target','0','--properties','{ node.name = sc_test_source }','/dev/zero'],env=env,stdout=subprocess.DEVNULL,stderr=graphlog)
  sink=subprocess.Popen(['pw-cat','--record','--raw','--rate','48000','--channels','2','--format','f32','--target','0','--properties','{ node.name = sc_test_destination }','-'],env=env,stdout=subprocess.DEVNULL,stderr=graphlog)
  time.sleep(.3)
  nodes=json.loads(subprocess.check_output(['pw-dump'],env=env))
  for n in nodes:
   name=n.get('info',{}).get('props',{}).get('node.name')
   if name in ['sc_test_source','sc_test_destination','soundcurrent_eq','soundcurrent_eq_output']:
    direction='Output' if name in ['sc_test_source','soundcurrent_eq_output'] else 'Input'
    subprocess.run(['pw-cli','set-param',str(n['id']),'PortConfig','{ direction = '+direction+' mode = dsp monitor = false control = false format = { mediaType = audio mediaSubtype = raw format = F32P rate = 48000 channels = 2 position = [ FL FR ] } }'],env=env,check=True,stdout=subprocess.DEVNULL)
  time.sleep(.3)
  inputs=subprocess.check_output(['pw-link','-i'],env=env,text=True).splitlines();outputs=subprocess.check_output(['pw-link','-o'],env=env,text=True).splitlines()
  for channel in ['FL','FR']:
   a=next(p for p in outputs if p.startswith('sc_test_source:') and p.endswith(channel))
   b=next(p for p in inputs if p.startswith('soundcurrent_eq:') and p.endswith(channel))
   subprocess.run(['pw-link',a,b],env=env,check=True)
   a=next(p for p in outputs if p.startswith('soundcurrent_eq_output:') and p.endswith(channel))
   b=next(p for p in inputs if p.startswith('sc_test_destination:') and p.endswith(channel))
   subprocess.run(['pw-link',a,b],env=env,check=True)
  time.sleep(.3)
  subprocess.run(['pw-cli','set-param',str(node['id']),'Props','{ params = [ "enhancements:Bass" 0.5 "enhancements:Ambience" 0.3 "enhancements:Dynamic" 0.2 ] }'],env=env,check=True,stdout=subprocess.DEVNULL)
  time.sleep(.3)
  updated=json.loads(subprocess.check_output(['pw-dump'],env=env));node=next(n for n in updated if n.get('id')==node['id'])
  controls={}
  for props in node['info']['params']['Props']:
   parameters=props.get('params',[]);controls.update(dict(zip(parameters[::2],parameters[1::2])))
  assert abs(controls['enhancements:Bass']-.5)<1e-6 and abs(controls['enhancements:Ambience']-.3)<1e-6, {k:v for k,v in controls.items() if k.startswith("enhancements:")}
  print('PASS: isolated PipeWire loads complete EQ + LADSPA graph and accepts live effect controls; no host devices or routes touched')
 finally:
  for p in [source,sink]:
   if p:p.terminate();p.wait(timeout=5)
  if graph:graph.terminate();graph.wait(timeout=5)
  server.terminate();server.wait(timeout=5)
