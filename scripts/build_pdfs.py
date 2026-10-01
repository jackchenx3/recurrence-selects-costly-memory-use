"""Render the Markdown manuscript and supplement as readable A4 PDFs.

Follows the established Markdown/reportlab rendering style. Figure references of the
form ![...](figures/<name>.png) are resolved by file name inside --figures-dir, which
must contain the outputs of scripts/make_figures.py; a missing figure is an error.
Relative (non-figure) links are rendered as plain labels so that no repository address
or release tag is embedded. Both --figures-dir and --output-dir are required.
"""
from pathlib import Path
import argparse, html, re
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (SimpleDocTemplate,Paragraph,Spacer,Image,KeepTogether,
                                Table,TableStyle,Preformatted)
import reportlab

ROOT=Path(__file__).resolve().parents[1]
TITLE='Environmental recurrence selects a costly memory-use allele without a resolved one-bit population benefit'
SHORT_TITLE='Recurrence and a costly memory-use allele'
VERSION_TEXT='Research preprint draft; not externally peer reviewed'
FONTROOT=Path(reportlab.__file__).parent/'fonts'
for name,file in [('Research','Vera.ttf'),('ResearchBold','VeraBd.ttf'),
                  ('ResearchItalic','VeraIt.ttf'),('ResearchMono','Vera.ttf')]:
    pdfmetrics.registerFont(TTFont(name,str(FONTROOT/file)))
pdfmetrics.registerFontFamily('Research',normal='Research',bold='ResearchBold',italic='ResearchItalic',boldItalic='ResearchBold')
NAVY=colors.HexColor('#17354b');GRAY=colors.HexColor('#55636e')
S={
 'body':ParagraphStyle('body',fontName='Research',fontSize=9.8,leading=14.7,spaceAfter=8,textColor=colors.HexColor('#20262c')),
 'title':ParagraphStyle('title',fontName='ResearchBold',fontSize=18,leading=23,spaceAfter=18,textColor=NAVY),
 'h2':ParagraphStyle('h2',fontName='ResearchBold',fontSize=13,leading=17,spaceBefore=16,spaceAfter=8,textColor=NAVY,keepWithNext=True),
 'h3':ParagraphStyle('h3',fontName='ResearchBold',fontSize=11,leading=15,spaceBefore=13,spaceAfter=7,textColor=NAVY,keepWithNext=True),
 'caption':ParagraphStyle('caption',fontName='Research',fontSize=8.6,leading=12.2,spaceAfter=12,textColor=GRAY),
 'reference':ParagraphStyle('reference',fontName='Research',fontSize=9,leading=12,spaceAfter=6,textColor=colors.HexColor('#20262c')),
 'bullet':ParagraphStyle('bullet',fontName='Research',fontSize=9.8,leading=14.2,spaceAfter=5,leftIndent=14,bulletIndent=3,textColor=colors.HexColor('#20262c')),
 'table':ParagraphStyle('table',fontName='Research',fontSize=8,leading=10.8),
 'code':ParagraphStyle('code',fontName='ResearchMono',fontSize=8.1,leading=11.6,spaceAfter=10,leftIndent=9),
}
WIDTH=A4[0]-100

def normalized(text):
    for mark in ['‐','‑','‒','–','—','−']:
        text=text.replace(mark,'-')
    return text.replace(' ',' ')

def inline(text):
    text=html.escape(normalized(text))
    def link(match):
        label,url=match.group(1),html.unescape(match.group(2))
        if not url.startswith(('https://','http://')):
            return label
        return '<link href="'+html.escape(url,quote=True)+'" color="#176b8c">'+label+'</link>'
    text=re.sub(r'\[([^\]]+)\]\(([^)]+)\)',link,text)
    text=re.sub(r'\*\*(.+?)\*\*',r'<b>\1</b>',text)
    text=re.sub(r'(?<![\w*])\*([^*]+?)\*(?![\w*])',r'<i>\1</i>',text)
    text=re.sub(r'`([^`]+)`',r'<font name="ResearchMono">\1</font>',text)
    return text

def footer(canvas,doc):
    canvas.saveState();canvas.setStrokeColor(colors.HexColor('#d5dde3'))
    canvas.line(50,37,A4[0]-50,37);canvas.setFont('Research',7.3);canvas.setFillColor(GRAY)
    canvas.drawString(50,25,SHORT_TITLE+' | '+VERSION_TEXT)
    canvas.drawRightString(A4[0]-50,25,str(doc.page));canvas.restoreState()

def column_widths(rows):
    cols=len(rows[0])
    weights=[]
    for c in range(cols):
        longest=max(len(r[c].getPlainText()) for r in rows if c<len(r))
        weights.append(min(max(longest,6),48))
    total=float(sum(weights))
    return [WIDTH*w/total for w in weights]

def resolve_figure(target,figures_dir):
    path=(figures_dir/Path(target).name).resolve()
    if not path.exists():
        raise SystemExit('missing generated figure: '+str(path)+' (run scripts/make_figures.py first)')
    return path

def render(source,dest,figures_dir):
    lines=source.read_text(encoding='utf-8').splitlines();story=[];i=0
    while i<len(lines):
        line=lines[i].strip()
        if not line:i+=1;continue
        if line.startswith('```'):
            code=[];i+=1
            while i<len(lines) and not lines[i].startswith('```'):code.append(lines[i]);i+=1
            story.append(Preformatted(normalized('\n'.join(code)),S['code']));i+=1;continue
        if line.startswith('|'):
            rows=[]
            while i<len(lines) and lines[i].strip().startswith('|'):
                cells=[c.strip() for c in lines[i].strip().strip('|').split('|')]
                if not all(re.fullmatch(r':?-+:?',c or ' ') for c in cells):
                    rows.append([Paragraph(inline(c),S['table']) for c in cells])
                i+=1
            table=Table(rows,colWidths=column_widths(rows),repeatRows=1,hAlign='LEFT')
            table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#e9eff4')),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),6),('RIGHTPADDING',(0,0),(-1,-1),6),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6),('LINEBELOW',(0,0),(-1,0),.6,NAVY),('LINEBELOW',(0,1),(-1,-1),.3,colors.HexColor('#dde4e8'))]))
            story.extend([table,Spacer(1,12)]);continue
        match=re.fullmatch(r'!\[([^\]]*)\]\(([^)]+)\)',line)
        if match:
            path=resolve_figure(match.group(2),figures_dir)
            image=Image(str(path));ratio=min(WIDTH/image.imageWidth,430/image.imageHeight)
            image.drawWidth=image.imageWidth*ratio;image.drawHeight=image.imageHeight*ratio
            group=[Spacer(1,8),image,Spacer(1,8)];i+=1
            while i<len(lines) and not lines[i].strip():i+=1
            if i<len(lines) and re.match(r'Figure \d+\.',lines[i]):
                group.append(Paragraph(inline(lines[i]),S['caption']));i+=1
            story.append(KeepTogether(group));continue
        if line.startswith('# '):style='title';text=line[2:]
        elif line.startswith('## '):style='h2';text=line[3:]
        elif line.startswith('### '):style='h3';text=line[4:]
        elif re.match(r'^Table \d+\.',line):style='caption';text=line
        elif re.match(r'^\d+\. ',line):style='reference';text=line
        elif line.startswith('- '):
            story.append(Paragraph(inline(line[2:]),S['bullet'],bulletText='•'));i+=1;continue
        else:
            style='body';parts=[line];i+=1
            while i<len(lines) and lines[i].strip() and not lines[i].lstrip().startswith(('#','|','```','![','- ')):
                parts.append(lines[i].strip());i+=1
            story.append(Paragraph(inline(' '.join(parts)),S[style]));continue
        story.append(Paragraph(inline(text),S[style]));i+=1
    title=lines[0][2:] if lines and lines[0].startswith('# ') else TITLE
    doc=SimpleDocTemplate(str(dest),pagesize=A4,rightMargin=50,leftMargin=50,topMargin=45,bottomMargin=49,
                          title=title,author='Jack Chen',
                          subject=VERSION_TEXT+': abstract computational population model')
    doc.build(story,onFirstPage=footer,onLaterPages=footer)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--figures-dir',type=Path,required=True,help='directory containing figure1_design.png, figure2_allele.png and figure3_accuracy.png')
    parser.add_argument('--output-dir',type=Path,required=True,help='directory for MANUSCRIPT.pdf and SUPPLEMENT.pdf')
    parser.add_argument('--source-root',type=Path,default=ROOT,help='package root containing paper/ (default: this package)')
    args=parser.parse_args();args.output_dir.mkdir(parents=True,exist_ok=True)
    for stem in ['figure1_design','figure2_allele','figure3_accuracy']:
        resolve_figure(stem+'.png',args.figures_dir)
    for name in ['MANUSCRIPT','SUPPLEMENT']:
        dest=args.output_dir/(name+'.pdf');render(args.source_root/'paper'/(name+'.md'),dest,args.figures_dir);print(dest)

if __name__=='__main__':main()
