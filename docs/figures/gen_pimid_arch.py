#!/usr/bin/env python3
"""
PIMID grand system-architecture figure (supersedes old Fig. 1).
Grounded in source (github.com/isaacyhe/pimid):
  - host<->device co-sim = SINGLE-PROCESS, same-thread domain transition via
    the ROI offload region  (NOT socket IPC).
  - network is COMPLETELY GARNET-based; in-memory DRAM net = hierarchical H-tree;
    simple vs detailed = analytical-vs-cycle-accurate fidelity.
  - fine-grained PE placement levels, addressing, core types, workloads, YAML.
Compact vertical layout + enlarged fonts.
Outputs: figures/PIMID_arch.pdf  and  /tmp/PIMID_new.png
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle, FancyArrowPatch
from matplotlib.lines import Line2D

C_HOST="#dbe7f3"; C_DEV="#e8f3e8"; C_CORE="#ffffff"; C_PE="#f6c9c0"
C_SUB="#eef3fb"; C_SUBD="#eef7ee"; C_INFRA="#f4f4f4"; C_PLUG="#ffffff"
C_IN="#fff3d6"; EDGE="#333333"; MUTE="#8a8a8a"; BLU="#1b3a5b"; GRN="#1f5130"; ORG="#9a5b16"

fig, ax = plt.subplots(figsize=(15, 6.05))
ax.set_xlim(3.2,96.8); ax.set_ylim(45.0,96.7); ax.axis("off")

def box(x,y,w,h,text="",fc=C_PLUG,ec=EDGE,fs=10,bold=False,lw=1.5,ls="-",tc="black",
        va="center",z=3):
    ax.add_patch(FancyBboxPatch((x,y),w,h,boxstyle="round,pad=0.02,rounding_size=0.5",
                 fc=fc,ec=ec,lw=lw,ls=ls,zorder=z))
    if text: ax.text(x+w/2,y+h/2,text,ha="center",va=va,fontsize=fs,
                     fontweight="bold" if bold else "normal",color=tc,zorder=z+1)
    return (x+w/2,y+h/2)

def txt(x,y,s,fs=9,bold=False,color="black",ha="center",va="center",it=False,z=6):
    ax.text(x,y,s,ha=ha,va=va,fontsize=fs,fontweight="bold" if bold else "normal",
            color=color,style="italic" if it else "normal",zorder=z)

def arrow(x1,y1,x2,y2,color=EDGE,lw=2.0,ls="-",style="-|>",mut=16,z=7):
    ax.add_patch(FancyArrowPatch((x1,y1),(x2,y2),arrowstyle=style,mutation_scale=mut,
                 lw=lw,color=color,ls=ls,zorder=z))

# ===== TOP INPUT BAND =====
box(4,91.2,42,5.4,fc=C_IN,lw=1.4)
txt(25,94.6,"Configuration File (YAML)",fs=12,bold=True)
txt(25,92.3,"engines · PE placement · memory technology · network · power",fs=8.8)
box(54,91.2,42,5.4,fc=C_IN,lw=1.4)
txt(75,94.6,"Workloads  (Shared Memory / Message Passing)",fs=11,bold=True)
txt(75,92.3,"µkernels · BabelStream · Rodinia · NPB · SPLASH-3 · PARSEC",fs=8.8)
arrow(25,90.9,25,89.5,color=ORG,lw=1.8,mut=9); arrow(75,90.9,75,89.5,color=ORG,lw=1.8,mut=9)

# ===== SINGLE-PROCESS WRAPPER =====
ax.add_patch(FancyBboxPatch((3.5,64.5),93,24.7,boxstyle="round,pad=0.02,rounding_size=0.6",
             fc="#fbfbfb",ec=BLU,lw=1.7,ls=(0,(6,3)),zorder=1))
txt(50,88.3,"Single simulation process — host & device accounting domains  (QEMU user-mode + ZSim)",
    fs=11.5,bold=True,color=BLU,va="top")

# ---------- HOST ENGINE ----------
box(4.5,66.2,38,19.7,fc=C_HOST,lw=1.7)
txt(23.5,84.30,"Host Simulation Engine",fs=13.5,bold=True,color=BLU)
cxh=[9.6,18.9,28.2,37.5]
for cx in cxh: box(cx-3.8,79.64,7.6,3.0,"Core",fc=C_CORE,fs=10.5,bold=True)
txt(23.5,76.94,"core_type:  ooo / in-order / simple",fs=9.0,it=True,color=BLU)
bx=[5.6,18.0,30.4]; bw=10.7
box(bx[0],66.84,bw,7.5,fc=C_SUB); txt(bx[0]+bw/2,72.09,"Caches",fs=10.2,bold=True); txt(bx[0]+bw/2,68.99,"L1+L2",fs=8.4,it=True,color=BLU)
box(bx[1],66.84,bw,7.5,fc=C_SUB); txt(bx[1]+bw/2,72.09,"Network",fs=10.2,bold=True); txt(bx[1]+bw/2,68.99,"GARNET",fs=8.4,it=True,color=BLU)
box(bx[2],66.84,bw,7.5,"Memory\nController",fc=C_SUB,fs=10.2,bold=True)

# ---------- DEVICE ENGINE ----------
box(57.5,66.2,38,19.7,fc=C_DEV,lw=1.7)
txt(76.5,84.30,"Device Simulation Engine",fs=13.5,bold=True,color=GRN)
cxd=[62.6,71.9,81.2,90.5]
for cx in cxd: box(cx-3.8,79.64,7.6,3.0,"PE",fc=C_PE,fs=10.5,bold=True)
txt(76.5,77.74,"pe_type:  ooo / in-order / simple / alu / null",fs=9.0,it=True,color=GRN)
txt(76.5,76.04,"placement:  subarray ▸ bank ▸ bank-group ▸ chip ▸ rank ▸ logic-die",fs=8.2,bold=True,color=GRN)
box(59.5,66.84,17,7.5,fc=C_SUBD,lw=1.4)
txt(68.0,72.39,"Address Translation",fs=9.4,bold=True); txt(68.0,70.59,"TLB + page table",fs=8.0,it=True,color=GRN); txt(68.0,68.79,"unified / discrete",fs=8.0,it=True,color=GRN)
box(77.5,66.84,16.5,7.5,fc=C_SUBD,lw=1.4)
txt(85.75,72.39,"In-memory Network",fs=9.4,bold=True); txt(85.75,70.59,"GARNET",fs=8.0,it=True,color=GRN); txt(85.75,68.79,"(hierarchical H-tree for DRAM)",fs=7.4,it=True,color=GRN)

# ---------- offload-sync transition ----------
gx=50
box(46.0,80.4,8.0,5.0,"Event Queue",fc="#ffffff",fs=9.2,bold=True,ls=(0,(3,2)))
# shared by BOTH engines: short connectors into each engine box
arrow(45.9,82.9,42.6,82.9,color=MUTE,lw=1.6,style="<|-|>",mut=11)
arrow(54.1,82.9,57.4,82.9,color=MUTE,lw=1.6,style="<|-|>",mut=11)
txt(gx,78.66,"offload region (ROI)",fs=9.6,bold=True,color=BLU)
arrow(43.0,75.16,57.0,75.16,color=BLU,lw=2.4,mut=15)
txt(gx,76.56,"host → device",fs=8.0,it=True,color=BLU)
arrow(57.0,71.46,43.0,71.46,color=BLU,lw=2.4,mut=15)
txt(gx,72.86,"device → host",fs=8.0,it=True,color=BLU)
txt(gx,67.9,"H/D transfers are charged on\nlink + memory models\n(PCIe · CXL · interposer)",fs=7.2,color=ORG,bold=True)

# arrows to modeling infrastructure
arrow(23.5,64.2,23.5,62.8,color=EDGE,lw=1.8,mut=9)
arrow(76.5,64.2,76.5,62.8,color=EDGE,lw=1.8,mut=9)

# ===== MODELING INFRASTRUCTURE =====
box(3.5,45.82,93,16.68,fc=C_INFRA,lw=1.7)
txt(50,61.5,"Modeling Infrastructure",fs=15,bold=True,va="top")
box(7,54.45,86,3.8,"Common Interface for External Models (YAML)",
    fc="#ffffff",fs=10,bold=True,ls=(0,(3,2)))
plugs=[
 ("DRAM","Ramulator2"),
 ("SRAM","CACTI"),
 ("NVM","NVSim"),
 ("Compute","ZSim / QEMU"),
 ("Network","GARNET"),
 ("Power & Area","McPAT"),
]
pcx=[14.17,28.5,42.83,57.17,71.5,85.83]; pw=13.5; pcy=50.44
for cx,(name,tool) in zip(pcx,plugs):
    box(cx-pw/2,pcy-3.25,pw,6.5,fc=C_PLUG,lw=1.4)
    txt(cx,pcy+1.1,name,fs=9.5,bold=True)
    txt(cx,pcy-1.1,"("+tool+")",fs=8.0,it=True,color="#555555")

import os
_here = os.path.dirname(os.path.abspath(__file__))
plt.savefig(os.path.join(_here, "PIMID_arch.pdf"), bbox_inches="tight", pad_inches=0.04)
plt.savefig(os.path.join(_here, "PIMID_arch.png"), dpi=140, bbox_inches="tight", pad_inches=0.04)
print("wrote PIMID_arch.pdf and PIMID_arch.png next to this script")
