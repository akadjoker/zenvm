
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
// Constantes
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

#define max_exp 512	  // M ximo nฃmero de elementos en una expresiขn
#define long_pila 2048	  // Longitud de la pila en ejecuciขn

#define swap(a,b) {a^=b;b^=a;a^=b;}

//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
// Mnemขnico/Cขdigo/Operandos (Generaciขn de cขdigo EML, "*" ๐ "aฃn no usado")
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

#define lnop  0 // *            No operaciขn
#define lcar  1 // valor        Carga una constante en pila
#define lasi  2 //              Saca valor, offset y mete el valor en [offset]
#define lori  3 //              Or lขgico
#define lxor  4 //              Xor, or exclusivo
#define land  5 //              And lขgico, operador sobre condiciones
#define ligu  6 //              Igual, operador logico de comparaciขn
#define ldis  7 //              Distinto, true si los 2 valores son diferentes
#define lmay  8 //              Mayor, comparaciขn con signo
#define lmen  9 //              Menor, idem
#define lmei 10 //              Menor o igual
#define lmai 11 //              Mayor o igual
#define ladd 12 //              Suma dos constantes
#define lsub 13 //              Resta, operaciขn binaria
#define lmul 14 //              Multiplicaciขn
#define ldiv 15 //              Divisiขn de enteros
#define lmod 16 //              Mขdulo, resto de la divisiขn
#define lneg 17 //              Negaciขn, cambia de signo una constante
#define lptr 18 //              Pointer, saca offset y mete [offset]
#define lnot 19 //              Negaciขn binaria, bit a bit
#define laid 20 //              Suma id a la constante de la pila
#define lcid 21 //              Carga id en la pila
#define lrng 22 // offset, len  Realiza una comparaciขn de rango
#define ljmp 23 // offset       Salta a una direcciขn de mem[]
#define ljpf 24 // offset       Salta si un valor es falso a una direcciขn
#define lfun 25 // cขdigo       Llamada a un proceso interno, ej. signal()
#define lcal 26 // offset       Crea un nuevo proceso en el programa
#define lret 27 // num_par      Auto-eliminaciขn del proceso
#define lasp 28 //              Desecha un valor apilado
#define lfrm 29 // num_par      Detiene por este frame la ejecuciขn del proceso
#define lcbp 30 // num_par      Inicializa el puntero a los par metros locales
#define lcpa 31 //              Saca offset, lee par metro [offset] y bp++
#define ltyp 32 // bloque       Define el tipo de proceso actual (colisiones)
#define lpri 33 // offset       Salta a la direcciขn, y carga var. privadas
#define lcse 34 // offset       Si switch <> expresiขn, salta al offfset
#define lcsr 35 // offset       Si switch no esta en el rango, salta al offset
#define lshr 36 //              Shift right (binario)
