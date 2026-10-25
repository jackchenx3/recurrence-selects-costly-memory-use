"""Plot canonical accepted estimates; no scientific programs or raw records."""
from pathlib import Path
import csv,json,hashlib,datetime
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
P=Path(__file__).resolve().parent
rows={r['endpoint']:r for r in csv.DictReader((P/'SIX_ENDPOINTS.tsv').open(),delimiter='\t')}
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'pdf.fonttype':42,'svg.fonttype':'none','axes.spines.top':False,'axes.spines.right':False})
fig,axes=plt.subplots(3,1,figsize=(9.5,7.4))
colors={'rec':'#24689c','inn':'#bc703c'}
titles={'L':'L  Eligible-lineage survivor share','P':'P  Normalized survivor accuracy','F':'F  Population-normalized focal-family contribution'}
for ax,q in zip(axes,'LPF'):
 ax.axvline(0,color='#656565',linewidth=.9,zorder=0)
 delta=100/(256 if q=='F' else 64)
 for sign in(-1,1):ax.axvline(sign*delta,color='#a7a7a7',linestyle=(0,(2,3)),linewidth=.9,zorder=0)
 bounds=[]
 for suffix,y in [('rec',1),('inn',0)]:
  r=rows[q+'_'+suffix];m=float(r['mean_percentage_points']);lo=float(r['lo_percentage_points']);hi=float(r['hi_percentage_points']);bounds.extend([lo,hi]);ax.errorbar(m,y,xerr=[[m-lo],[hi-m]],fmt='o',markersize=6.4,capsize=4,color=colors[suffix],linewidth=1.7)
  ax.text(1.025,y,f'{m:+.3f}  [{lo:+.3f}, {hi:+.3f}]',transform=ax.get_yaxis_transform(),va='center',fontsize=9,color='#252525',clip_on=False)
 pad=(max(bounds)-min(bounds))*.10;ax.set_xlim(min(bounds)-pad,max(bounds)+pad);ax.set_ylim(-.62,1.65);ax.set_yticks([1,0],['Recurrent','Innovation']);ax.tick_params(axis='y',length=0,pad=7);ax.set_title(titles[q],loc='left',fontsize=11,fontweight='semibold',pad=9);ax.xaxis.set_major_locator(MaxNLocator(6));ax.set_xlabel('CACHE − '+('FOCAL_FRESH'if q=='F'else'ALL_FRESH')+'  (percentage points)',fontsize=9);ax.grid(axis='x',alpha=.12);ax.spines['left'].set_visible(False)
axes[0].text(1.025,1.81,'Estimate [simultaneous interval]',transform=axes[0].get_yaxis_transform(),fontsize=9,fontweight='semibold',clip_on=False)
fig.suptitle('Immediate retrieval effects at paired checkpoint targets',x=.12,y=.98,ha='left',fontsize=14,fontweight='semibold')
fig.text(.12,.030,'n = 42,000 blocks; fixed equal ALL_F/ALL_M mixture. Intervals jointly ≥95% under the protocol’s\nindependent-uniform-word idealization. Dotted lines: ± material threshold δ. F is unconditional.',fontsize=9,color='#444444',linespacing=1.5)
fig.subplots_adjust(left=.15,right=.69,top=.875,bottom=.15,hspace=.84)
for ext in ['pdf','svg','png']:fig.savefig(P/('SIX_ENDPOINTS_FOREST.'+ext),dpi=180,facecolor='white')
plt.close(fig)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(P/'FIGURE_GENERATION_RECEIPT.json').write_text(json.dumps({'at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'PresentationonlyofalreadyEAacceptedcanonicalresults; no newtests/statistics/rawrecords/scientificprogramruns','input_tsv_sha256':h(P/'SIX_ENDPOINTS.tsv'),'plot_source_sha256':h(Path(__file__)),'matplotlib_version':matplotlib.__version__,'isolated_plot_environment':'/tmp/bit002-publication-plot-env-20261025','outputs':{ext:h(P/('SIX_ENDPOINTS_FOREST.'+ext))for ext in ['pdf','svg','png']}},indent=2)+'\n')
