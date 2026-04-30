
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
// Nucleo de la ejecuciขn de los programas en DIV
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

case lnop: break;
case lcar: pila[++sp]=mem[ip++]; break;
case lasi: pila[sp-1]=mem[pila[sp-1]]=pila[sp]; sp--; break;
case lori: pila[sp-1]|=pila[sp]; sp--; break;
case lxor: pila[sp-1]^=pila[sp]; sp--; break;
case land: pila[sp-1]&=pila[sp]; sp--; break;
case ligu: pila[sp-1]=pila[sp-1]==pila[sp]; sp--; break;
case ldis: pila[sp-1]=pila[sp-1]!=pila[sp]; sp--; break;
case lmay: pila[sp-1]=pila[sp-1]>pila[sp]; sp--; break;
case lmen: pila[sp-1]=pila[sp-1]<pila[sp]; sp--; break;
case lmei: pila[sp-1]=pila[sp-1]<=pila[sp]; sp--; break;
case lmai: pila[sp-1]=pila[sp-1]>=pila[sp]; sp--; break;
case ladd: pila[sp-1]+=pila[sp]; sp--; break;
case lsub: pila[sp-1]-=pila[sp]; sp--; break;
case lmul: pila[sp-1]*=pila[sp]; sp--; break;
case ldiv:
  #ifdef DEBUG
    if (pila[sp]==0) {
      pila[--sp]=0; v_function=-2; e(145);
      if (call_to_debug) { process_stoped=id; return; }
    } else { pila[sp-1]/=pila[sp]; sp--; }
  #else
    pila[sp-1]/=pila[sp]; sp--;
  #endif
  break;
case lmod:
  #ifdef DEBUG
    if (pila[sp]==0) {
      pila[--sp]=0; v_function=-2; e(145);
      if (call_to_debug) { process_stoped=id; return; }
    } else { pila[sp-1]%=pila[sp]; sp--; }
  #else
    pila[sp-1]%=pila[sp]; sp--;
  #endif
  break;
case lneg: pila[sp]=-pila[sp]; break;
case lptr: pila[sp]=mem[pila[sp]]; break;
case lnot: pila[sp]^=-1; break;
case laid: pila[sp]+=id; break;
case lcid: pila[++sp]=id; break;
case lrng:
  #ifdef DEBUG
    if (pila[sp]<0 || pila[sp]>mem[ip]) {
      v_function=-2; e(140);
      if (call_to_debug) { ip++; process_stoped=id; return; }
    } ip++;
  #else
    ip++;
  #endif
  break;
case ljmp:
  ip=mem[ip];
  #ifdef DEBUG
    if (reloj>max_reloj) {
      v_function=-2; e(142); max_reloj=max_process_time+reloj;
      if (call_to_debug) { process_stoped=id; return; }
    }
  #endif
  break;
case ljpf:
  if (pila[sp--]&1) ip++; else ip=mem[ip];
  #ifdef DEBUG
    if (reloj>max_reloj) {
      v_function=-2; e(142); max_reloj=max_process_time+reloj;
      if (call_to_debug) { process_stoped=id; return; }
    }
  #endif
  break;
case lfun:
  function();
  #ifdef DEBUG
    if (call_to_debug) { process_stoped=id; return; }
  #endif
  break;
case lcal:
  #ifdef DEBUG
  process_exec(id,get_ticks()-oreloj); oreloj=get_ticks();
  #endif
  mem[id+_IP]=ip+1; id2=id; if (sp>long_pila) exer(3);
  procesos++; ip=mem[ip]; id=id_start;
  while (mem[id+_Status] && id<=id_end) id+=iloc_len;
  if (id>id_end) { if (id>imem_max-iloc_len) exer(2); id_end=id; }
  memcpy(&mem[id],&mem[iloc],iloc_pub_len<<2);
  mem[id+_Id]=id;
  if (mem[id+_BigBro]=mem[id2+_Son]) mem[mem[id+_BigBro]+_SmallBro]=id;
  mem[id2+_Son]=id; mem[id+_Father]=mem[id+_Caller]=id2;
  if (mem[ip+2]==lnop) mem[id+_FCount]=mem[id2+_FCount]+1; // Funciขn
  #ifdef DEBUG
  else process_level++;
  #endif
  break;
case lret:
  #ifdef DEBUG
  if (mem[id+_FCount]==0) process_level--;
  process_exec(id,get_ticks()-oreloj); oreloj=get_ticks();
  #endif
  sp=mem[id+_Param]-mem[id+_NumPar];
  pila[sp]=id; id=mem[id+_Caller];
  elimina_proceso(pila[sp]);
  if (!(id&1)) goto next_process1;
  ip=mem[id+_IP];
  break;
case lasp:
  sp--;
  break;
case lfrm:
  #ifdef DEBUG
  process_exec(id,get_ticks()-oreloj); oreloj=get_ticks();
  #endif
  sp=mem[id+_Param]-mem[id+_NumPar];
  mem[id+_IP]=ip;
  next_frm:
  mem[id+_IdScan]=0; mem[id+_BlScan]=0;
  pila[sp]=id; id=mem[id+_Caller];
  if (!(id&1)) goto next_process1;
  ip=mem[id+_IP];
  if (mem[pila[sp]+_FCount]>0) { // Si era una funcion, duerme al padre.
    mem[pila[sp]+_Caller]++; // Caller dormido
    bp=sp; // Deja la pila aparcada en el lugar adecuado
    sp=mem[id+_Param]-mem[id+_NumPar]; // Y la prerara para retornar al anterior
    guarda_pila(id,sp,bp);
    mem[id+_Status]=3; goto next_frm;
  } else {
    #ifdef DEBUG
    process_level--;
    #endif
    mem[pila[sp]+_Caller]=0;
    mem[pila[sp]+_Executed]=1;
  } break;
case lcbp:
  mem[id+_NumPar]=mem[ip++];
  mem[id+_Param]=sp-mem[id+_NumPar]+1;
  break;
case lcpa: mem[pila[sp--]]=pila[mem[id+_Param]++]; break;
case ltyp: mem[id+_Bloque]=mem[ip++]; inicio_privadas=mem[6]; break;
case lpri:
  memcpy(&mem[id+inicio_privadas],&mem[ip+1],(mem[ip]-ip-1)<<2);
  inicio_privadas+=(mem[ip]-ip-1); ip=mem[ip]; break;
case lcse:
  if (pila[sp-1]==pila[sp]) ip++; else ip=mem[ip];
  sp--; break;
case lcsr:
  if (pila[sp-2]>=pila[sp-1] && pila[sp-2]<=pila[sp]) ip++;
  else ip=mem[ip]; sp-=2; break;
case lshr: pila[sp-1]>>=pila[sp]; sp--; break;
case lshl: pila[sp-1]<<=pila[sp]; sp--; break;
case lipt:
		pila[sp]=++mem[pila[sp]];
	break;
case lpti:
		pila[sp]=mem[pila[sp]]++;
	break;
case ldpt:
		pila[sp]=--mem[pila[sp]]; break;
	break;
case lptd:
		pila[sp]=mem[pila[sp]]--;
	break;
case lada: pila[sp-1]=mem[pila[sp-1]]+=pila[sp]; sp--; break;
case lsua: pila[sp-1]=mem[pila[sp-1]]-=pila[sp]; sp--; break;
case lmua: pila[sp-1]=mem[pila[sp-1]]*=pila[sp]; sp--; break;
case ldia:
  #ifdef DEBUG
    if (pila[sp]==0) {
      mem[pila[sp-1]]=0;
