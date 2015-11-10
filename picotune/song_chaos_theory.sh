python3 picotune '[S("w",t>>9),S("k",32),S("m",2048),S("a",1-(t/m)%1),
    S("d",(14*t*t^t)%m*a),S("p",int(w/k)&3),S("y",[3,3,4.7,2][p]*t/4),
    S("h",ord("IQNNNN!!]]!Q!IW]WQNN??!!W]WQNNN?"[w>>1&15|int(p/3)<<4])/33*t-t),
    S("s",y*.98%80+y%80+((a*((int(5*t%m*a)&128)*((0x53232323>>(w//4%32)&1))+
        (int(d)&127)*(0xa444c444>>(w//4%32)&1)*1.5+(int(d*w)&1)+
        (h%k+h*1.99%k+h*.49%k+h*.97%k-64)*(4-a-a))) if int(w)>>7 else 0)),
    127 if int(s*s)>>14 else int(s)][-1]' --format S8
