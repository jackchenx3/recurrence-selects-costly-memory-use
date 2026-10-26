from pathlib import Path
from fractions import Fraction
from decimal import Decimal,localcontext,ROUND_HALF_EVEN
import json,re,hashlib,datetime
import sys
N=Path(__file__).resolve().parents[1];A=Path(sys.argv[1]).resolve()if len(sys.argv)>1 else N/'data/bit003';raw=(A/'RESULTS_A.txt').read_bytes();assert raw==(A/'RESULTS_B.txt').read_bytes();lines=raw.decode('ascii').splitlines();assert raw.endswith(b'\n')and len(lines)==11 and lines[0]=='BIT003_RESULTS_V1';pairs=[s.split('=',1)for s in lines[1:]];assert [k for k,v in pairs]==['status','tier','n_blocks','n_all_f','n_all_m','policy_signed_sum','policy_denominator','accuracy_signed_sum','accuracy_denominator','inference'];v=dict(pairs);assert v['status']=='VALID'and v['tier']=='production-r1'and v['inference']=='PRODUCTION_ONLY'
for k in ['n_blocks','n_all_f','n_all_m','policy_signed_sum','policy_denominator','accuracy_signed_sum','accuracy_denominator']:assert re.fullmatch(r'(0|-?[1-9][0-9]*)',v[k]);v[k]=int(v[k])
assert v['n_blocks']==20000 and v['n_all_f']==v['n_all_m']==10000 and v['policy_denominator']==20000*8192 and v['accuracy_denominator']==20000*262144
frac=lambda x:{'numerator':x.numerator,'denominator':x.denominator}
def decimal_nearest(x,n=12):
 with localcontext()as c:c.prec=70;return format((Decimal(x.numerator)/Decimal(x.denominator)).quantize(Decimal(10)**(-n),rounding=ROUND_HALF_EVEN),f'.{n}f')
def outward(x,upper=False,n=12):
 a=x.numerator*10**n;b=x.denominator;z=-((-a)//b)if upper else a//b;sgn='-'if z<0 else'';z=abs(z);return f'{sgn}{z//10**n}.{z%10**n:0{n}d}'
def classify(lo,hi):
 d=Fraction(1,32)
 if lo>d:return 'MATERIAL_POSITIVE'
 if hi<-d:return 'MATERIAL_NEGATIVE'
 if lo>=-d and hi<=d:return 'BOUNDED_WITHIN_MARGIN'
 if lo>0:return 'POSITIVE_MAGNITUDE_UNRESOLVED'
 if hi<0:return 'NEGATIVE_MAGNITUDE_UNRESOLVED'
 return 'UNRESOLVED'
endpoints={}
for name,key,h,alpha in [('primary_policy_frequency','policy',19778834661,'0.04'),('secondary_survivor_accuracy','accuracy',23018074131,'0.01')]:
 x=Fraction(v[key+'_signed_sum'],v[key+'_denominator']);hw=Fraction(h,10**12);lo=max(Fraction(-1),x-hw);hi=min(Fraction(1),x+hw);endpoints[name]={'signed_sum':v[key+'_signed_sum'],'denominator':v[key+'_denominator'],'estimate_exact':frac(x),'estimate_decimal_nearest_12':decimal_nearest(x),'fixed_halfwidth_exact':frac(hw),'alpha':alpha,'interval_lower_exact':frac(lo),'interval_upper_exact':frac(hi),'interval_lower_decimal_outward_12':outward(lo),'interval_upper_decimal_outward_12':outward(hi,True),'classification_from_exact_rationals_before_rounding':classify(lo,hi),'estimate_percentage_points_nearest_6':decimal_nearest(x*100,6),'interval_lower_percentage_points_outward_6':outward(lo*100,n=6),'interval_upper_percentage_points_outward_6':outward(hi*100,True,n=6),'meaningful_threshold_exact':frac(Fraction(1,32)),'equivalence_margin_exact':frac(Fraction(1,32))}
z={'at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'study':'PHASE3-BIT-SUSTAINED-RETRIEVAL-003','original_job_id':'54339475','G8_sha256':hashlib.sha256((N/'studies/bit003/provenance/G8_EXECUTION_ACCEPTANCE.json').read_bytes()).hexdigest(),'unseal_authority_sha256':'See original private unseal receipt; no new unseal or scientificexecution required for arithmetic presentation','original_results_sha256':hashlib.sha256(raw).hexdigest(),'original_results_bytes':len(raw),'n_blocks':20000,'all_f_blocks':10000,'all_m_blocks':10000,'estimand':'CACHE-minus-fresh-substitution total sustained intervention effect over256turnovers with ownbranchstatefeedback; equalALL_F/ALL_Mmixture','coverage':'Two fixed simultaneousHoeffding intervals joint>=95% conditionalonidealindependentindexedRNG; alphafamily0.04+0.01','endpoint_definition_and_inference_changed_after_unseal':False,'postproduction_scientific_program_invocations':0,'endpoints':endpoints}
(A/'EXACT_ANALYSIS.json').write_text(json.dumps(z,indent=2)+'\n');(A/'PRESENT_RESULTS_EXACT.py').write_bytes(Path(__file__).read_bytes());print(json.dumps(endpoints,indent=2))
