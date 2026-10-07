#!/usr/bin/env python3
"""Oracle check: is the gap to the paper caused by the matching code or by tree-position noise?

For every ETH pair, the target tree list is kept, but every target tree that has a
mutual nearest source tree within 0.3 m (2D, after the GT transform) is replaced by
the GT-transformed source tree:
  xyz : x, y and z replaced  -> distances between matched trees are exact
  xy  : only x, y replaced, z kept from the target scan -> tree-base height noise stays
Unmatched trees stay as they are. Then cm_register runs at the paper's 5 cm.
Diagnostic only (uses GT); not a method.
Usage: python3 scripts/eth_trees/oracle_check.py <empty work dir>
"""
import csv, math, os, subprocess, sys

R=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))); raw=R+"/data/eth_trees/raw/trees"; runs=R+"/results/eth_trees/runs"; S=sys.argv[1]
def load(p): return [(float(r["x"]),float(r["y"]),float(r["z"])) for r in csv.DictReader(open(p))]
def tfm(p): return [[float(v) for v in l.split()] for l in open(p) if l.strip()][:3]
ap=lambda T,p: tuple(sum(T[i][j]*p[j] for j in range(3))+T[i][3] for i in range(3))
pairs=[l.split() for l in open(raw+"/pairs.txt") if l.strip()]
for mode in ("xyz","xy"):
  for prof in ("baseline","improved"):
    res=[]
    for sa,sb in pairs:
      A=load(f"{runs}/{sa}/{prof}/step_raw_seed1_trees.csv"); B=load(f"{runs}/{sb}/{prof}/step_raw_seed1_trees.csv")
      T=tfm(f"{raw}/groundtruth/{sa}-{sb}.tfm"); At=[ap(T,p) for p in A]
      nn=lambda p,Q: min(range(len(Q)),key=lambda j:math.hypot(p[0]-Q[j][0],p[1]-Q[j][1]))
      B2=list(B); nm=0
      for i,p in enumerate(At):
        j=nn(p,B)
        if math.hypot(p[0]-B[j][0],p[1]-B[j][1])<0.3 and nn(B[j],At)==i:
          B2[j]=p if mode=="xyz" else (p[0],p[1],B[j][2]); nm+=1
      f=f"{S}/{mode}_{prof}_{sb}_for_{sa}.csv"
      with open(f,"w") as o: o.write("x,y,z\n"+"".join(f"{x},{y},{z}\n" for x,y,z in B2))
      out=subprocess.run([R+"/build/cm_register",f"--src_trees={runs}/{sa}/{prof}/step_raw_seed1_trees.csv",f"--tgt_trees={f}",
        f"--src_cloud={raw}/{sa}.ply",f"--gt={raw}/groundtruth/{sa}-{sb}.tfm","--thresholds=0.05"],capture_output=True,text=True,check=True).stdout
      for r in csv.DictReader(out.splitlines()):
        res.append((r["solver"],f"{sa}-{sb}",nm,int(r["n_matches"]),int(r["n_correct_matches"]),float(r["rot_err_deg"]),float(r["e_p_m"]),int(r["success"])))
    for sv in ("svd","svd+ransac"):
      s=[x for x in res if x[0]==sv]
      ep=sorted(x[6] for x in s if not math.isnan(x[6]))
      print(f"{mode:3} {prof:8} {sv:10} success {sum(x[7] for x in s)}/10  e_p median {ep[len(ep)//2] if ep else float('nan'):.3f}  matches/correct "+" ".join(f"{x[3]}/{x[4]}" for x in s))
