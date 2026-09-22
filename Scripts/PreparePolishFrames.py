"""Read alpha only; writes frame rectangles, never writes or edits image pixels.
Connected silhouettes keep long weapons intact even when a generated pose exceeds its nominal cell.
"""
from pathlib import Path
import json,hashlib
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'ArtSource/StorybookV1/Polish'

def components(mask):
    parents=[];runs=[];previous=[]
    def find(i):
        while parents[i]!=i:
            parents[i]=parents[parents[i]];i=parents[i]
        return i
    for y,row in enumerate(mask):
        change=np.diff(np.r_[False,row,False].astype(np.int8))
        current=[];j=0
        for left,right in zip(np.flatnonzero(change==1),np.flatnonzero(change==-1)):
            left,right=int(left),int(right);index=len(parents);parents.append(index)
            while j<len(previous) and previous[j][1]<left:j+=1
            k=j
            while k<len(previous) and previous[k][0]<=right:
                parents[find(previous[k][2])]=find(index);k+=1
            current.append((left,right,index));runs.append((left,right,y,index))
        previous=current
    groups={}
    for left,right,y,index in runs:
        key=find(index)
        if key not in groups:groups[key]=[left,y,right,y+1,0]
        b=groups[key];b[0]=min(b[0],left);b[1]=min(b[1],y);b[2]=max(b[2],right);b[3]=max(b[3],y+1);b[4]+=right-left
    return sorted(groups.values(),key=lambda b:b[4],reverse=True)

def main():
    catalog=json.loads((SRC/'catalog.json').read_text(encoding='utf-8-sig'));report=[]
    legacy={'Hero_Knight':.2,'Hero_Mage':.5,'Hero_Ranger':.5,'Unit_Swordsman':.5,'Unit_Archer':.5,'Unit_Shieldbearer':.5,'Building_ArrowTower':.5,'Building_Barracks':.5}
    for row in catalog['animations']:
        path=ROOT/row['file']
        if not path.exists():continue
        im=Image.open(path);a=np.asarray(im.getchannel('A'));h,w=a.shape;cw,ch=w/4,h/4
        pieces=components(a>96);cells=[[] for _ in range(16)]
        for box in pieces:
            if box[4]<cw*ch*.0005:continue
            x=(box[0]+box[2])/2;y=(box[1]+box[3])/2
            cell=min(3,int(y/ch))*4+min(3,int(x/cw));cells[cell].append(box)
        assert all(cells),(row['id'],'Empty frame')
        bounds=[]
        for index,group in enumerate(cells):
            main=group[0];b=list(main[:4])
            # Retain accessories/rubble inside the same cell, but not detached flying arrows.
            for box in group[1:]:
                gap=max(b[0]-box[2],box[0]-b[2],b[1]-box[3],box[1]-b[3],0)
                if gap<=5 and box[4]>main[4]*.01:
                    b=[min(b[0],box[0]),min(b[1],box[1]),max(b[2],box[2]),max(b[3],box[3])]
            b=[max(0,b[0]-2),max(0,b[1]-2),min(w,b[2]+2),min(h,b[3]+2)]
            bounds.append(b)
        ref=Image.open(ROOT/row['reference']);rb=ref.getchannel('A').getbbox()
        reference_visible=(rb[3]-rb[1])/ref.height*row['worldHeight']
        idle_height=float(np.median([b[3]-b[1] for b in bounds[:4]]))
        scale=legacy.get(row['id'],1.0);ppu=idle_height/reference_visible*scale
        # Preserve the old sprite's foot position relative to its centre pivot.
        foot_world=(rb[3]-ref.height/2)/ref.height*row['worldHeight']
        frames=[]
        for i,b in enumerate(bounds):
            # UE 5.8 CustomPivotPoint is in the ENTIRE texture's pixel space, not SourceUV-relative.
            # Alpha median follows the body rather than a long weapon; all poses share a foot baseline.
            yy,xx=np.nonzero(a[b[1]:b[3],b[0]:b[2]]>96)
            pivot=[b[0]+float(np.median(xx)),b[3]-foot_world*ppu/scale]
            frames.append(dict(index=i,uv=b[:2],size=[b[2]-b[0],b[3]-b[1]],pivot=pivot))
        report.append(dict(id=row['id'],file=row['file'],sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            width=w,height=h,ppu=ppu,spriteScaleY=scale,frames=frames,states=row['states'],
            sourceReferenceSha256=hashlib.sha256((ROOT/row['reference']).read_bytes()).hexdigest(),
            transparentFraction=float(np.mean(a==0)),opaqueFraction=float(np.mean(a==255))))
    (SRC/'frames.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('FRAME_RECTS',len(report),'/',len(catalog['animations']))
if __name__=='__main__':main()
