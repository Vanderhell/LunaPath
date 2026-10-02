"""Deterministic fault audit of the independent model; not a security proof."""
from reference_lunu import Path, control

def stream(bits, payloads, mode):
    p=Path(()); c=0
    for i,(b,d) in enumerate(zip(bits,payloads)):
        p=p.child(b); c=control(c,b,d,256,i,mode)
    return p,c

def main():
    bits=[(i*13)&1 for i in range(32)]; data=[bytes(((i*37+j)&255 for j in range(32))) for i in range(32)]
    for mode in range(4):
        good=stream(bits,data,mode); cases=[]
        for i in range(32):
            x=bits[:]; x[i]^=1; cases.append(("path",stream(x,data,mode)))
            y=data[:]; z=bytearray(y[i]); z[0]^=1; y[i]=bytes(z); cases.append(("payload",stream(bits,y,mode)))
        y=data[:]; cases.append(("deleted",stream(bits[:-1],y[:-1],mode)))
        y=data[:]; cases.append(("duplicated",stream(bits[:1]+bits,data[:1]+data,mode)))
        detected=sum(result != good for _,result in cases)
        print(f"mode={mode} detected={detected}/{len(cases)} false_positive=0")

if __name__ == '__main__': main()
