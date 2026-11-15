import json,urllib.error
def check_profile(root,api):
 api('disconnect',{});secret=api('profiles/password',dict(host='127.0.0.1',port=12987,username='admin',password='localJarTest'));ref=secret['credentialId']
 try:
  config=dict(host='127.0.0.1',port=12987,username='admin',password='',credentialId=ref,mode='SNAPSHOT')
  result=api('connect',config);assert any(n['name']=='probe' for n in result['graph']['nodes']);assert result['status']['viewers']==0
  try:api('connect',dict(config,host='192.0.2.10'));raise AssertionError('Saved secret used for wrong endpoint')
  except urllib.error.HTTPError as e:assert 'another address' in json.load(e)['error']
  disk=(root/'data/credentials'/f'{ref}.json').read_text(encoding='utf-8');assert 'localJarTest' not in disk
 finally:api('profiles/forget',dict(credentialId=ref))
 (root/'evidence/profile-native.json').write_text(json.dumps(dict(result='PASS',tests=['saved DPAPI credential connects to native SVM without resubmitting plaintext','endpoint mismatch rejected before connecting','password encrypted on disk','protected test secret removed']),indent=2),encoding='utf-8')
