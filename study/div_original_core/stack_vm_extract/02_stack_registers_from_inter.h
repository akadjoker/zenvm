//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
//  Memoria de la m quina destino
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

GLOBAL int pila[long_pila+max_exp+64]; // c lculo de expresiones (compilaciขn y ejecuciขn)

GLOBAL int * mem, imem, iloc, iloc_pub_len, iloc_len;
GLOBAL byte * memb;
GLOBAL word * memw;

//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
// Variables globales para la interpretaciขn - VARIABLES DE PROCESO
//อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ

GLOBAL int inicio_privadas; // Inicio de variables privadas (proceso en ejecuciขn)

GLOBAL int ip;        // Puntero de programa

GLOBAL int sp;          // Puntero de pila

GLOBAL int bp;          // Puntero auxiliar de pila

GLOBAL int id_init;     // Inicio del proceso init (padre de todos)

GLOBAL int id_start;    // Inicio del primer proceso (sus locales y privadas)

GLOBAL int id_end;      // Inicio del ฃltimo proceso hasta el momento

GLOBAL int id_old;      // Para saber por donde se est  procesando

GLOBAL int procesos;    // Nฃmero de procesos vivos en el programa
