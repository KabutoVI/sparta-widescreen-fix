"""List/extract Sparta (AWE) .pak files. Layout from Vfs.dll CPakBuilder::WriteVolume (own reading of the pseudocode):
header 0x14 bytes: 'PAK ', version, f158, f154, count. Bytes from file position 8 onward are XORed with the
5-byte pattern 19 a6 c9 d2 cd indexed by (pos-8)%5. Index entry: u32 name_len, name bytes, 5 x u32
(offset, ?, ?, size, ?). File data follows; stored through the same XOR keyed by absolute file position."""
import sys, struct, os
KEY=bytes([0x19,0xa6,0xc9,0xd2,0xcd])
def dec(buf, pos):
    return bytes(b ^ KEY[(pos+i-8)%5] if pos+i>=8 else b for i,b in enumerate(buf))
def read_index(f):
    f.seek(0); h=dec(f.read(20),0)
    magic,ver,a,b,count=struct.unpack('<4sIIII',h)
    assert magic==b'PAK ', magic
    pos=20; ents=[]
    for i in range(count):
        f.seek(pos); n=struct.unpack('<I',dec(f.read(4),pos))[0]
        if n>1000: raise SystemExit('bad name length %d at entry %d pos %d'%(n,i,pos))
        raw=dec(f.read(n+20),pos+4)
        name=raw[:n].decode('latin1'); vals=struct.unpack('<5I',raw[n:n+20])
        ents.append((name,)+vals); pos+=4+n+20
    return h,ents,pos
if __name__=='__main__':
    p=sys.argv[1]
    with open(p,'rb') as f:
        h,ents,end=read_index(f)
        print('header',struct.unpack('<4sIIII',h),'entries',len(ents),'index ends at',end)
        for e in ents[:int(sys.argv[2]) if len(sys.argv)>2 else 15]: print(e)
