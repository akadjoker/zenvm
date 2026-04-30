// Funciones:
//      guarda_pila(id,sp,mem[id+_SP]); // En lfrm y lfrf de funciones
//      carga_pila(id);                 // En exec_process/trace_process
//      actualiza_pila(id,valor);       // Pone un valor al final de la pila (rtf)

// * (int *) mem[id+_SP] = {SP1,SP2,DATOS...}

void guarda_pila(int id, int sp1, int sp2) {
  int n, * p;
  p=(int*)malloc((sp2-sp1+3)*sizeof(int));
  if (p!=NULL) {
    mem[id+_SP]=(int)p; p[0]=sp1; p[1]=sp2;
    for (n=0;n<=sp2-sp1;n++) p[n+2]=pila[sp1+n];
  } else mem[id+_SP]=0;
}

void carga_pila(int id) {
  int n, * p;
  if (mem[id+_SP]) {
    p=(int*)mem[id+_SP];
    for (n=0;n<=p[1]-p[0];n++) pila[p[0]+n]=p[n+2];
    mem[id+_SP]=0; sp=p[1];
    free(p);
  } else sp=0;
}

void actualiza_pila(int id, int valor) {
  int * p;
  if (mem[id+_SP]) {
    p=(int*)mem[id+_SP];
    p[p[1]-p[0]+2]=valor;
  }
}

//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
// Intrprete del cขdigo generado
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

int max,max_reloj;        // Para procesar segฃn _Priority y pintar segฃn _Z
extern int alt_x;

void interprete (void)
{
  inicializacion();
  while (procesos && !(kbdFLAGS[_ESC] && kbdFLAGS[_L_CTRL]) && !alt_x) {
    error_vpe=0;
    frame_start();
    #ifdef DEBUG
    if (kbdFLAGS[_F12] || trace_program) {
      trace_program=0;
      if (debug_active) call_to_debug=1;
    }
    #endif
    old_dump_type=dump_type;
    old_restore_type=restore_type;
    do {
      #ifdef DEBUG
      if (call_to_debug) { call_to_debug=0; debug(); }
      #endif
      exec_process();
    } while (ide);
    frame_end();
    if (error_vpe!=0) {
      // printf("Error: %s\n",error_vpe);
      v_function=-2; e(error_vpe);
    }
  }
  finalizacion();
}

//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
// Procesa el siguiente proceso
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

int oreloj;

void exec_process(void) {
  #ifdef DEBUG
  oreloj=get_ticks();
  #endif

  ide=0; max=0x80000000;

  #ifdef DEBUG
  if (process_stoped) {
    id=ide=process_stoped;
    process_stoped=0;
    goto continue_process;
  }
  #endif

  id=id_old; do {
    if (mem[id+_Status]==2 && !mem[id+_Executed] &&
        mem[id+_Priority]>max) { ide=id; max=mem[id+_Priority]; }
    if (id==id_end) id=id_start; else id+=iloc_len;
  } while (id!=id_old);


  if (ide) if (mem[ide+_Frame]>=100) {
    mem[ide+_Frame]-=100;
    mem[ide+_Executed]=1;
  }
  else {

    _net_loop(); // Recibe los paquetes justo antes de ejecutar el proceso~

    id=ide; ip=mem[id+_IP]; carga_pila(id);

    #ifdef DEBUG
    continue_process:
    #endif

    max_reloj=reloj+max_process_time;

    nucleo_exec();

    id=ide; if (post_process!=NULL) post_process();
  }
}

//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
//  Nฃcleo interno del intrprete
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

int oo; // Para usos internos en el kernel

void nucleo_exec() {

	do {
	  switch ((byte)mem[ip++]){
      #include "kernel.cpp"
    }
 	} while (1);

  next_process1: mem[ide+_Executed]=1;
  next_process2: ;

}

//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
// Procesa la siguiente instruccion del siguiente proceso
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

#ifdef DEBUG

void trace_process(void) {
  #ifdef DEBUG
  oreloj=get_ticks();
  #endif

  ide=0; max=0x80000000;
