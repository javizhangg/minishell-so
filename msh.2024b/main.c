/*-
 * main.c
 * Minishell C source
 * Shows how to use "obtain_order" input interface function.
 *
 * Copyright (c) 1993-2002-2019, Francisco Rosales <frosal@fi.upm.es>
 * Todos los derechos reservados.
 *
 * Publicado bajo Licencia de Proyecto Educativo Práctico
 * <http://laurel.datsi.fi.upm.es/~ssoo/LICENCIA/LPEP>
 *
 * Queda prohibida la difusión total o parcial por cualquier
 * medio del material entregado al alumno para la realización
 * de este proyecto o de cualquier material derivado de este,
 * incluyendo la solución particular que desarrolle el alumno.
 *
 * DO NOT MODIFY ANYTHING OVER THIS LINE
 * THIS FILE IS TO BE MODIFIED
 */

#include <stddef.h>			/* NULL */
#include <stdio.h>			/* setbuf, printf */
#include <stdlib.h>         //memoria dinámica (malloc)
#include <unistd.h>         //Funciones de sistema tipo exec fork
#include <fcntl.h>          //control de archivos
#include <sys/types.h>      //tipos de datos del sistema pid
#include <sys/wait.h>       //majo de hijos con wait
#include <signal.h>         // SIGNAL
#include <string.h>         //manejo de cadenas
#include <sys/stat.h>       //informa y permisos de archivos
#include <sys/times.h> 		// time
#include <time.h>           //tiempo y fehcas
#include <pwd.h> 		    // Para getpwnam
#include <ctype.h> 		    // Para isalnum
#include <dirent.h>         // Para opendir, readdir, closedir

extern int obtain_order();		/* See parser.y for description */

char *prompt = NULL; // Mensaje de apremio
pid_t bgpid = -1;    // PID del último proceso en background
int status = 0;      // Último estado de terminación
pid_t mypid;         // PID del minishell

//Funcion para ignorar señales especificas
void ignorarsignal() {
    struct sigaction sa;        //estructura para definir comportamiento de señales 
    sa.sa_handler = SIG_IGN;    // Ignorar señales
    sa.sa_flags = 0;            //no se aplica flags adicionales

    sigaction(SIGINT, &sa, NULL);//ignora control c
    sigaction(SIGQUIT, &sa, NULL);//ingonra el control 
}

//Funcion que restaura las señales
void restaurasignal() {
    struct sigaction sa;        //Establece estructura para definir el comportamiento de señales
    sa.sa_handler = SIG_DFL;    // Acción por defecto
    sa.sa_flags = 0;            //aplica flags a 0
    
    sigaction(SIGINT, &sa, NULL);//restablece la de control c
    sigaction(SIGQUIT, &sa, NULL);//restablece control 
}


/* Función para manejar el comando cd */
int funcion_cd(char **argv, char *filev[3]) {
    //A dicha funcion le ingresamos el comando en si y los ficheros de entrada y salida
    int statuscd = 0;       //almacena el status de cd
    int fd_in = -1;         //sirve para guardar descriptor entrada
    int fd_out = -1;        //sirve para guardar descripor salida
    int saved_stdin = -1;   //guarda la entrada estandar
    int saved_stdout = -1;  //gurada la salida estandar

    // Redirigir entrada estándar si se especifica un archivo
    if (filev[0] != NULL) {
        fd_in = open(filev[0], O_RDONLY);
        if (fd_in < 0) {
            perror("Error al abrir archivo de redirección de entrada");
            return 1;
        }
        saved_stdin = dup(STDIN_FILENO);
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }

    // Redirigir salida estándar si se especifica un archivo
    if (filev[1] != NULL) {
        fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_out < 0) {
            perror("Error al abrir archivo de redirección de salida");
            if (saved_stdin != -1) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            return 1;
        }
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }

    // Manejar argumentos de cd en los distintos casos que reciva
    if (argv[1] == NULL) {
        //si no recive argumento la funcion te lleva al directorio HOME
        statuscd = chdir(getenv("HOME"));
    } else if (argv[2] != NULL) {
        //si tiene segundo argumento da un error y devuleve entrada y salida estandar
        fprintf(stderr, "Error: cd recibe un máximo de 1 argumento\n");
        if (saved_stdin != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        return 1;
    } else {
        //si se recive directorio se intenta cambiar a ese directorio.
        statuscd = chdir(argv[1]);
    }

    if (statuscd != 0) {
        //sin no se ha podido cambiar devuelve entrada y salida estandar y impreme mensaje de error
        perror("Error al cambiar de directorio");
        if (saved_stdin != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        return 1;
    }

    // Mostrar el directorio actual despues del cambio
    char cwd[1024];//Buffer para almacenar el directorio actual
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        //obtiene con la funcion cwd el directorio actual y lo imprime 
        printf("%s\n", cwd);
    } else {
        //en caso de que no se pueda obtener la funcion, devuelve entrada y salida estandar y sale de la funcion
        perror("Error obteniendo el directorio actual");
        if (saved_stdin != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        return 1;
    }

    // Restaurar stdin y stdout si fueron redirigidos correctamente
    if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout != -1) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    return 0;
}
/* Función para manejar el comando interno umask */
int funcion_umask(char **argv, char *filev[3]) {
    /*El modelo de funcionamiento esta planteado de manera que sea parecido al cd */
    int fd_out = -1;        //guarda el descriptor de salida
    int fd_err = -1;        //guarda el descriptor de entrada
    int saved_stdout = -1;  //guarda la salida estandar
    int saved_stderr = -1;  //guarda la entrada estandar
    mode_t current_mask;    //lo utilizaremos para almacenamiento de la mascara

    // Redirigir salida estándar si se especifica un archivo
    if (filev[1] != NULL) {
        fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_out < 0) {
            perror("Error al abrir archivo de redirección de salida");
            return 1;
        }
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd_out, STDOUT_FILENO);
    }

    // Redirigir error estándar si se especifica un archivo
    if (filev[2] != NULL) {
        fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_err < 0) {
            perror("Error al abrir archivo de redirección de error");
            if (fd_out != -1) {
                close(fd_out);
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
            }
            return 1;
        }
        saved_stderr = dup(STDERR_FILENO);
        dup2(fd_err, STDERR_FILENO);
    }

    // Comprobar número de argumentos
    if (argv[1] != NULL && argv[2] != NULL) {
        fprintf(stderr, "Error: umask recibe un máximo de un argumento\n");
        if (fd_out != -1) {
            close(fd_out);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (fd_err != -1) {
            close(fd_err);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
        }
        return 1;
    }

    // Obtener y mostrar la máscara actual
    current_mask = umask(0);
    umask(current_mask); // Restaurar la máscara a su valor original
    printf("%04o\n", current_mask);//muestra mascara actual

    // Si se proporciona un nuevo valor, cambiar la máscara
    if (argv[1] != NULL) {
        char *endptr;
        long new_mask = strtol(argv[1], &endptr, 8); // Convertir el valor en un numero octal
        if (*endptr != '\0' || new_mask < 0 || new_mask > 0777) {
            //valida que sea un numero octal valido si no lo es devuelve entrada y salida estandar y sale de la funcion
            fprintf(stderr, "Error: El valor de umask debe ser un número octal válido entre 0000 y 0777\n");
            if (fd_out != -1) {
                close(fd_out);
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
            }
            if (fd_err != -1) {
                close(fd_err);
                dup2(saved_stderr, STDERR_FILENO);
                close(saved_stderr);
            }
            return 1;
        }
        //si es correcto se actualiza la mascara a al nueva mascara
        umask((mode_t)new_mask); // Establecer la nueva máscara
    }

    // Restaurar stdout y stderr si se redirigieron
    if (fd_out != -1) {
        close(fd_out);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (fd_err != -1) {
        close(fd_err);
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }

    return 0;
}

/* Función para manejar redirecciones */
int redirecionamiento(char *filev[3]) {
    /*Funcion que utilizaremos en casod de que exista un solo comando, lo unico que hace es redirecionar 
    los archivos de entrada o salida en caso de que haya*/
    if (filev[0]) {
        int fd_in = open(filev[0], O_RDONLY);
        if (fd_in < 0) {
            perror("Error abriendo archivo de entrada");
            return -1;
        }
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }
    if (filev[1]) {
        int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_out < 0) {
            perror("Error abriendo archivo de salida");
            return -1;
        }
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }
    if (filev[2]) {
        int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_err < 0) {
            perror("Error abriendo archivo de errores");
            return -1;
        }
        dup2(fd_err, STDERR_FILENO);
        close(fd_err);
    }
    return 0;
}

int esinterna(char **argv){
    /*Funcion sencilla que no hace mas que devolver 1 si es interna y 0 si no es interna lo utilizaresmos para 
    el main para comprobar si es una funcion interna o externa*/
	if(strcmp(argv[0],"cd")==0 ||strcmp(argv[0],"umask")==0  
	|| strcmp(argv[0],"time")==0  || strcmp(argv[0],"read")==0  ){

		return 1;
	}
	return 0;
}
/*implementacion de la funcion time*/
int funcion_time(char **argv, char *filev[3]) {
    struct tms time_empezar, time_acabar;//nos servira para almacera el tiempo de incio y el timepo de fin de proceso
    clock_t real_empezar, real_finalizar; //variables para registrar tiempos de reloj real

    int fd_out = -1, fd_err = -1;//son los descriptores de salida y error 
    int saved_stdout = -1, saved_stderr = -1;//guardaremos la salida de error estandary salida estandar 

    // Manejar redirecciones de salida estándar en caso de que haya
    if (filev[1]) {
        fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_out < 0) {
            perror("Error al abrir archivo de redirección de salida");
            return 1;
        }
        saved_stdout = dup(STDOUT_FILENO);
        dup2(fd_out, STDOUT_FILENO);
    }

    // Manejar redirección de errores en caso de que haya
    if (filev[2]) {
        fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_err < 0) {
            perror("Error al abrir archivo de redirección de error");
            if (fd_out != -1) {
                close(fd_out);
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
            }
            return 1;
        }
        saved_stderr = dup(STDERR_FILENO);
        dup2(fd_err, STDERR_FILENO);
    }

    // Registrar tiempos iniciales antes de ejecutar el comando
    real_empezar = times(&time_empezar);
    if (real_empezar == (clock_t)-1) {
        //En caso de que no se registre devuelve un error
        perror("Error obteniendo tiempos iniciales");
        return 1;
    }

    if (argv[1] == NULL) {
        // Si no hay argumentos, mostrar tiempo acumulado del minishell
        real_finalizar = times(&time_acabar);
        //Registramos el timpo final
        if (real_finalizar == (clock_t)-1) {
            perror("Error obteniendo tiempos finales");
            return 1;
        }

        long ticks_por_seg = sysconf(_SC_CLK_TCK);//Obtine los ticks por segundos

        //Calculasmos los distintos tiempos necesarios(usuarios, sistema y real), en segundo, minisegundos.
        long tiempo_user_segundos=(time_acabar.tms_utime + time_acabar.tms_cutime) / ticks_por_seg;
        long tiempo_user_miliseg=((time_acabar.tms_utime + time_acabar.tms_cutime) % ticks_por_seg) * 1000 / ticks_por_seg;
        long tiempo_sis_segundo=(time_acabar.tms_stime + time_acabar.tms_cstime) / ticks_por_seg;
        long tiempo_sis_miliseg=((time_acabar.tms_stime + time_acabar.tms_cstime) % ticks_por_seg) * 1000 / ticks_por_seg;
        long tiempo_real_segundos=(real_finalizar - real_empezar) / ticks_por_seg;
        long timepo_real_miliseg=((real_finalizar - real_empezar) % ticks_por_seg) * 1000 / ticks_por_seg;
        printf("Tiempo acumulado del minishell:\n");
        //Imprimimos el tiempo basandonos en el lo que nos dice el pdf
        printf("%ld.%03ld user %ld.%03ld system %ld.%03ld real\n",
               tiempo_user_segundos,tiempo_user_miliseg,
               tiempo_sis_segundo,tiempo_sis_miliseg,
               tiempo_real_segundos,timepo_real_miliseg);

    } else if (esinterna(&argv[1])) {
        // En caso de que sea un mandato interno, lo ejecutamos 
        int status = 0;
        //Ejecutamos el comando interno.
        if (strcmp(argv[1], "cd") == 0) {
            status = funcion_cd(&argv[1], filev);
        } else if (strcmp(argv[1], "umask") == 0) {
            status = funcion_umask(&argv[1], filev);
        } else if (strcmp(argv[1], "time") == 0) {
            status = funcion_time(&argv[1], filev); // Puede ser recursivo
        }

        // Registrar el tiempo final deespues de haber ejecutado el tiempo 
        real_finalizar = times(&time_acabar);
        if (real_finalizar == (clock_t)-1) {
            //En caso de qeu no se genere devuelve error 
            perror("Error obteniendo tiempos finales");
            return 1;
        }
        //se sigue el mismo funcionamiento anterior para imprimir el timepo
        long ticks_por_seg = sysconf(_SC_CLK_TCK);
        long tiempo_user_segundos=(time_acabar.tms_utime + time_acabar.tms_cutime) / ticks_por_seg;
        long tiempo_user_miliseg=((time_acabar.tms_utime + time_acabar.tms_cutime) % ticks_por_seg) * 1000 / ticks_por_seg;
        long tiempo_sis_segundo=(time_acabar.tms_stime + time_acabar.tms_cstime) / ticks_por_seg;
        long tiempo_sis_miliseg=((time_acabar.tms_stime + time_acabar.tms_cstime) % ticks_por_seg) * 1000 / ticks_por_seg;
        long tiempo_real_segundos=(real_finalizar - real_empezar) / ticks_por_seg;
        long timepo_real_miliseg=((real_finalizar - real_empezar) % ticks_por_seg) * 1000 / ticks_por_seg;
        printf("Tiempo del mandato interno:\n");
        printf("%ld.%03ld user %ld.%03ld system %ld.%03ld real\n",
               tiempo_user_segundos,tiempo_user_miliseg,
               tiempo_sis_segundo,tiempo_sis_miliseg,
               tiempo_real_segundos,timepo_real_miliseg);

        return status;

    } else {
        // Ejecutar el mandato externo ya que no es ni vacion ni interno
        pid_t pid = fork();//Creamos el hijo
        if (pid == 0) {
            // El proceso hijo ejecuta el comando externo
            execvp(argv[1], &argv[1]);
            perror("Error ejecutando el mandato");
            exit(1);
        } else if (pid > 0) {
            // En el proceso padre espera al proceso hijo
            int status;
            waitpid(pid, &status, 0);

            real_finalizar = times(&time_acabar);//despues de esperar al hijo registramos el tiempo final.
            if (real_finalizar == (clock_t)-1) {
                //En caso de que no se registre el tiempo da un error
                perror("Error obteniendo tiempos finales");
                return 1;
            }
            //Imprimimos el tiempo basandonos en lo anterior pero para mandatos externos.
            long ticks_por_seg = sysconf(_SC_CLK_TCK);
            long tiempo_user_segundos=(time_acabar.tms_utime + time_acabar.tms_cutime) / ticks_por_seg;
            long tiempo_user_miliseg=((time_acabar.tms_utime + time_acabar.tms_cutime) % ticks_por_seg) * 1000 / ticks_por_seg;
            long tiempo_sis_segundo=(time_acabar.tms_stime + time_acabar.tms_cstime) / ticks_por_seg;
            long tiempo_sis_miliseg=((time_acabar.tms_stime + time_acabar.tms_cstime) % ticks_por_seg) * 1000 / ticks_por_seg;
            long tiempo_real_segundos=(real_finalizar - real_empezar) / ticks_por_seg;
            long timepo_real_miliseg=((real_finalizar - real_empezar) % ticks_por_seg) * 1000 / ticks_por_seg;
            printf("Tiempo del mandato externo:\n");
            printf("%ld.%03ld user %ld.%03ld system %ld.%03ld real\n",
               tiempo_user_segundos,tiempo_user_miliseg,
               tiempo_sis_segundo,tiempo_sis_miliseg,
               tiempo_real_segundos,timepo_real_miliseg);

            return WEXITSTATUS(status);
        } else {
            perror("Error creando el proceso hijo");
            return 1;
        }
    }

    // Restaurar stdout y stderr si fueron redirigidosm en caso de no haber sino no hace nada
    if (fd_out != -1) {
        close(fd_out);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (fd_err != -1) {
        close(fd_err);
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }

    return 0;
}




/*Nos sirve para expandir los metacaracteres que se hayan leido en un comando*/
char* expandir_metacaracteres(const char* input) {
    
    if (!input) return NULL; //En el caso de que no haya input return Null

    char* expanded = malloc(strlen(input) + 1); //Se crea memoria dinamica para el metacaracter leido

    if (!expanded) {
        perror("Error en malloc");
        return NULL;
    }

    strcpy(expanded, input);    //copia los inputs en expanded

    // Manejo de ~ en el minishell

    if (input[0] == '~') {
        char* home_dir = NULL;  //inicializa el home_dir a nulo
        if (input[1] == '\0') {
            // Solo ~, usar el HOME del usuario actual
            home_dir = getenv("HOME");
        } else {
            // ~ seguido de un nombre de usuario
            struct passwd* pw;  //Creamos un passwd para obtener la informacion de un usuario
            char username[256]; //Buffer para almacenar el nombre de usuario.
            sscanf(input + 1, "%255s", username); //sirve para estaer el nombre de unsuario despues de la ~
            pw = getpwnam(username);    //da el puntero a pw que contiene la informacion del usuario.
            if (pw) {
                home_dir = pw->pw_dir;// si se encontro el usuario se obtiene su direcion y se obtine su directorio HOME
            } else {
                fprintf(stderr, "Usuario desconocido: %s\n", username); //en caso de que no se encuentre imprime un Mensaje de error
                free(expanded);
                return NULL;
            }
        }

        if (home_dir) {
            expanded = realloc(expanded, strlen(home_dir) + strlen(input)); //nos sirve para asegur que hay espacion suficiente para la direccion
            if (!expanded) {// en caso de que no se redimesione correctamente, imprinme un error
                perror("Error en realloc");
                return NULL;
            }
            strcpy(expanded, home_dir); //Copia el directorio HOME en el buffer expanded
        }
    }

    // Manejo de $ en el minishell
    char* dollar = strchr(expanded, '$'); //Busca el $ en la cadena expanded con strchr
    while (dollar) {//Mientras que existe el dolar
        char variable_name[256]; //Buffer para almacenar el nombre de la varible 
        char* end = dollar + 1; //Apuntar al caracter despues del $

        //sirve para avanzar hasta el final del nombre de la variable
        while (*end && (isalnum(*end) || *end == '_')) {
            end++;
        }

        size_t var_length = end - (dollar + 1); //Nos da la longitud del nombre de la variable
        strncpy(variable_name, dollar + 1, var_length);     //Copia el nombre de la variable en variable_name
        variable_name[var_length] = '\0';   //nos asegura que termina

        const char* value = getenv(variable_name);  //Obtiene el valor de la varible de entorno
        if (!value) value = ""; // Si no existe, usar cadena vacía

        size_t new_length = strlen(expanded) - (var_length + 1) + strlen(value); //calculal la longitud de la cadena expandida
        char* new_expanded = malloc(new_length + 1);    //Reserva en memoria la longitud de la cadena expandida
        
        if (!new_expanded) {
            //Si falla el malloc da un error
            perror("Error en malloc");
            free(expanded);
            return NULL;
        }

        strncpy(new_expanded, expanded, dollar - expanded); //copia la parte de la cadena antes del dolar
        new_expanded[dollar - expanded] = '\0'; //termina con \0
        strcat(new_expanded, value);            //Añade la variable de entorno a al nueva cadena
        strcat(new_expanded, end);              //Añade el resto de la cadena despues de la variable

        free(expanded);                         //libera la cadena anterior
        expanded = new_expanded;                //actualiza el expanded
        dollar = strchr(expanded, '$');         // Buscar el siguiente $
    }

    return expanded;  
}

/*Implementacion de la funcon read*/
int funcion_read(char **argv, char *filev[3]) {
    int fd_in = -1;//nos sirve para abrir el descriptor de entrada
    int saved_stdin = -1;//guardamos la entrada estandar.

    // Manejo de redirección de entrada si se especifica un archivo con el descriptor de entrada
    if (filev[0]) {
        fd_in = open(filev[0], O_RDONLY);
        if (fd_in < 0) {
            perror("Error al abrir archivo de redirección de entrada");
            return 1;
        }
        saved_stdin = dup(STDIN_FILENO);
        dup2(fd_in, STDIN_FILENO);
    }

    char line[1024];//Se crea un buffer para leer una linea de entrada
    if (!fgets(line, sizeof(line), stdin)) {//En caso de haya datos en la entrada estandar, se leen sino se piden al usuario
        perror("Error leyendo entrada");
        //En caso de no haberse podido leer se devuelve la entrada estandar si hay y se devuelve un error
        if (fd_in != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        return 1;
    }

    // Remover el salto de línea al final de la línea leída
    line[strcspn(line, "\n")] = '\0';

    // Si no se han pasado argumentos se envia un error ya que es necesario al menos una variable
    if (!argv[1]) {
        fprintf(stderr, "Error: read necesita al menos una variable\n");
        //imprime error y devuelve la entrada estandar
        if (fd_in != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        return 1; // Devuelve un error
    }

    //separa la linea en token separados por espacion o tabuladores
    char *token = strtok(line, " \t");
    int i = 1;//Contador para iterar los argumentos

    while (token) {
        // Si es la última variable que hemos metido le asignamos el resto de la linea a la variable
        if (argv[i + 1] == NULL) {
            // Construir la ultima cadena con la ultima variable
            char *texto_restante = strdup(token);
            token = strtok(NULL, ""); // Tomar todo el resto de la línea
            if (token) {
                texto_restante = realloc(texto_restante, strlen(texto_restante) + strlen(token) + 2);//redimensionamos el texto restante.
                if (!texto_restante) {
                    perror("Error en realloc");
                    if (fd_in != -1) {
                        dup2(saved_stdin, STDIN_FILENO);
                        close(saved_stdin);
                    }
                    return 1;
                }
                //añade el espacion y el resto del texto 
                strcat(texto_restante, " ");
                strcat(texto_restante, token);
            }

            // Construir la variable de entorno para guardar el valor
            char *var_entorno = malloc(strlen(argv[i]) + strlen(texto_restante) + 2);//se crea memoria dinamica para
            if (!var_entorno) {
                perror("Error en malloc");
                free(texto_restante);
                if (fd_in != -1) {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                return 1;
            }
            sprintf(var_entorno, "%s=%s", argv[i], texto_restante);//Añade el texto a la variable de entorno
            free(texto_restante);//loveramos de la memoria dinamica el texto restante porque ya lo hemos almacenado en la variable

            if (putenv(var_entorno) != 0) {
                //en caso de que putenv no funcione, se genera un error
                perror("Error asignando valor a variable de entorno");
                free(var_entorno);
                if (fd_in != -1) {
                    //en caso de que haya variables estandares de entrada se devuelven
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                return 1;
            }
            break; // Salir porque ya hemos procesado el resto
        }

        // Construir la variable para putenv en formato "VARIABLE=valor"
        char *var_entorno = malloc(strlen(argv[i]) + strlen(token) + 2);//Guardamos memoria dinamica para la variable de entorno
        if (!var_entorno) {
            //si no se guarda la memoria dinamica correctamente, da un error y devuleve la entrada estandar si habia
            perror("Error en malloc");
            if (fd_in != -1) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            return 1;
        }
        sprintf(var_entorno, "%s=%s", argv[i], token);//añade el valor al valor de entorno

        if (putenv(var_entorno) != 0) {
            //en cas o de que no se haya asignado correctamente da un error 
            perror("Error asignando valor a variable de entorno");
            free(var_entorno);
            if (fd_in != -1) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
            return 1;
        }

        // Avanzar al siguiente token y argumento
        token = strtok(NULL, " \t");
        i++;
    }

    // Restaurar entrada estándar si fue redirigida
    if (fd_in != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }

    return 0;
}

// Actualiza las variables especiales después de la ejecución de comandos
void actualizar_variables_especiales() {
    // Actualizar mypid con el ID del proceso actual
    if (!mypid) {// si mypid no ha sido inicializada lo inicializa
        mypid = getpid(); //obtiene ID proceso actual
        char mypid_str[16];//Crea un buffer para almacenar el ID como una cadena
        snprintf(mypid_str, sizeof(mypid_str), "%d", mypid); //convierte el ID del proceso a cadena
        setenv("mypid", mypid_str, 1);// Establece la variable mypid con el ID del proceso actual
    }

    // Actualizar bgpid con ID del proceso en segundo plano
    if (bgpid != -1) {
        /*En el caso de que no haya proceso de segundo plano se elimina la variable bgpid 
        en el else, pero en el caso de que haya proceso en segundo plano*/
        char bgpid_str[16]; //se cra el buffer para almacenar el ID del proceso en segundo plano
        snprintf(bgpid_str, sizeof(bgpid_str), "%d", bgpid);//convierte el bgpid en una cadena
        setenv("bgpid", bgpid_str, 1);//estaablece la variable de bgpid con el ID del proceso en segundo plano actual
    } else {
        unsetenv("bgpid");//se elimina la variable bgpid de 
    }

    // Actualizar status con el estado del ultimo comando ejecutado
    char status_str[16];//Buffer para almacenar el estado con una cadena.
    snprintf(status_str, sizeof(status_str), "%d", status);//Convierte el status en cadena
    setenv("status", status_str, 1);//se establece la variable status en el valor del estado del ulitmo comando
}

// Inicializa el mensaje de apremio (prompt)
void inicializar_prompt() {
    prompt = getenv("prompt");//intenta obtener la variable de entorno de prompt.
    if (!prompt) {
        //en caso de no ser definida se utiliza msh> si esta definida utiliza la predeterminada
        prompt = "msh> ";
    }
}

// Imprime el mensaje de apremio
void imprimir_prompt() {
    //llama a la funcion de inicializar pront que en caso de que exista ya el prompt no hace nada y si no tiene lo sustituye.
    inicializar_prompt();
    fprintf(stderr, "%s", prompt);//imprime el prompt
}

// Realiza la expansión de variables especiales en un comando
void expandir_variables_especiales(char **argv) {
    //Recorremos el comando para ver si encontramso un $ buscamos el valor de la variable de entorno correspondiente
    for (int i = 0; argv[i] != NULL; i++) {
        if (argv[i][0] == '$') {
            const char *valor = getenv(argv[i] + 1);//Toma el valor del nombre ignorando el $
            if (valor) {
                argv[i] = strdup(valor); // Sustituir por el valor de la variable
            }
        }
    }
}

void expandir_comodines(char **argv) {
    for (int i = 0; argv[i] != NULL; i++) {
        // Comprueba si tiene un "?" pero no tiene "/"
        if (strchr(argv[i], '?') != NULL && strchr(argv[i], '/') == NULL) {
            
            DIR *dir = opendir(".");    //nos abre el directorio actual
            if (!dir) {
                //Si no se ha podido abrir el dir salta el error
                perror("Error abriendo directorio");
                continue;
            }

            struct dirent *entrada;       //almacena entrada al directorio 
            char *patron = argv[i];    //guarda el parton de ?
            size_t longitud_patron = strlen(patron);   //nos da la longitul del patron
            char **coincidencias = NULL;  // Lista que almacena las coincidencias
            int cont_coincidencias = 0;    //cuenta las coincidencias

            while ((entrada = readdir(dir)) != NULL) {
                //recorremos cada entrada del directorio
                if (strlen(entrada->d_name) != longitud_patron) {
                    continue;  // Ignorar nombres con longitud diferente al del patron
                }

                // Comprobar coincidencia con el patrón
                int match = 1;//inicializamos a 1 para si hay coincidencia
                for (size_t j = 0; j < longitud_patron; j++) {
                    //si el caracter no es "?" y no coincide con el correspondiente del nombre no se genera un match
                    if (patron[j] != '?' && patron[j] != entrada->d_name[j]) {
                        match = 0;
                        break;
                    }
                }

                if (match) {
                    //Comprobamos si hay match, en el caso de que haya añadimos el nombre del archivo a la lista de coincidencias.
                    coincidencias = realloc(coincidencias, (cont_coincidencias + 1) * sizeof(char *));  //redimensionamos las conincidencias
                    if (!coincidencias) {//no se ejecute correctamente el realloc
                        perror("Error en realloc");
                        closedir(dir);
                        return;
                    }
                    coincidencias[cont_coincidencias++] = strdup(entrada->d_name);
                }
            }

            closedir(dir); //se cierra el directorio despues de procesar todas las entradas

            if (cont_coincidencias > 0) {
                // Expandir el argumento en caso de coincidencia
                coincidencias = realloc(coincidencias, (cont_coincidencias + 1) * sizeof(char *)); // se redimensiona
                coincidencias[cont_coincidencias] = NULL; //terminamos las conincidencias con null

                free(argv[i]);  // Liberar el argumento original
                argv[i] = coincidencias[0];  // El primer match reemplaza el argumento

                // Insertamos  los restantes después del actual
                for (int j = 1; j < cont_coincidencias; j++) {
                    int k = 0;
                    while (argv[k] != NULL) k++;  // Buscamos el final del array 

                    argv = realloc(argv, (k + 2) * sizeof(char *)); //redimensionamos el argv para añadirle la nueva entrada
                    if (!argv) {
                        perror("Error en realloc");
                        return;
                    }
                    //se desplaza los elementos existenetes para hacer espacio
                    memmove(&argv[i + j + 1], &argv[i + j], (k - i - j + 1) * sizeof(char *));
                    argv[i + j] = coincidencias[j];//se añaden las coincidencias al array
                }
            }

            // Liberar de memoria las coincidencias
            if (coincidencias) {
                free(coincidencias);
            }
        }
    }
}


int main(void)
{

	// Ignorar SIGINT y SIGQUIT en el minishell
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

	char ***argvv = NULL;                       //Array de array de comandos
	int argvc;                                  //Cantidad de comandos
	int tamtotal;                               //Totalde comandos, lo utilizaresmos para guardar la cantidad total de comandos que nos da el usuario
	char **argv = NULL;                         //Sirve apra guardar el array del comando, compuesto por el mandato y argumenetos
	int argc;                                   //Nos sirve para recorrer el comando como tal
	char *filev[3] = { NULL, NULL, NULL };      //Almacena los nombres de archivos para redirecionamiento file[0] entrada,file[1] salida y file[2] salida error
	int bg;                                     //Indica si el comando es de tipo segundo plano
	int ret;                                    /*analiza la línea de entrada del usuario, 0 es Eof, -1 error en sintaxis y >0 comandos detectados*/

	setbuf(stdout, NULL);			/* Unbuffered en la salida de estandar*/
	setbuf(stdin, NULL);            /* Unbuffered en la salida estandar*/

	while (1) {
		imprimir_prompt();          /*Muestra el Prompt del minishell que en nuestro caso es msh>*/  
		//fprintf(stderr, "%s", "msh> ");	
		ret = obtain_order(&argvv, filev, &bg);     //Lee los comandos ingresados y separa los comandos en argvv, los redirecionamientos en filev y 
                                                    //si se detecta segundo plano se actiavara bg
		if (ret == 0) {
            /*En el caso de que ret =0 se llega eof y se acaba*/
            printf("eof\n");
            break;
        }
        if (ret == -1) {
            /*En el caso de que ret =-1 existe un syntax error y se acaba*/
            printf("Syntax error\n");
            continue;
        }
        argvc = ret - 1; //ret -1 nos da el numero de comandos que tenemos y lo almacenamos en argvc
        if (argvc == 0) {
            //si el numero de comandos es 0 se llega al linea vacia y vuelve a pedir comandos al usuario
            printf("Empty line\n");
            continue;
        }


		tamtotal=argvc;//almaceno en tamtotal el numero total de comandos que ha introducido el usuario.
		
		pid_t last_pid;     //Sirve para guardar el ultimo pid ulitilizado y nos servira para el correcto funcionamiento del pipe
		int fd[2];         // Sirve para general PIPES
		int fd_prev[2]={-1,-1};     // Almacena el descriptor de lectura del pipe anterior
		
        /*En el for lo que hacemos es recorrer los comandos de uno en uno y despues dentrod el for tenemos 2 casos uno que es en caso de que haya solo un comando y 
        en el caso de que haya mas de un comadno*/

		for (argvc = 0; (argv = argvv[argvc]); argvc++) {

			for (argc = 0; argv[argc]; argc++){
				char* expanded_arg = expandir_metacaracteres(argv[argc]); //Nos sirve apra expandir los metacaracteres 
    			if (expanded_arg) {
       				free(argv[argc]);           // Liberar el argumento original
        			argv[argc] = expanded_arg;  // Usar el argumento expandido
    			} else {
        			fprintf(stderr, "Error expandiendo metacaracteres en: %s\n", argv[argc]);
    			}
				expandir_comodines(argv);  // Expande nombres de fichero
				printf("%s ", argv[argc]);/*En el caso de tener el caracter comodin se expandira y 
                se immprimira el comando a ejecutary en caso de no tener na da imprimira el comando a ejecutar*/
    
            }       

			printf("\n");//imprimimos un salto de linea para que quede mas estetico
            /*Yo voy a distingir dos casos en el caso de que haya mas de un comando y en el caso de que solo haya un
            comando*/
			if (tamtotal>1){
				expandir_variables_especiales(argv);//espandimos las variables especiales
				
				if (esinterna(argv)==1 &&argvc == tamtotal - 1) { // Si es el último comando de un pipe y es interno se ejecuta en el proceso principal
                    int saved_stdin = dup(STDIN_FILENO); // Guardar stdin original
                    if (argvc > 0) { // Redirigir entrada desde el pipe anterior
                        dup2(fd_prev[0], STDIN_FILENO); // Conectar la entrada estándar al pipe
                        close(fd_prev[0]);//cierra el pipe previo
                        close(fd_prev[1]);
                    }
                    //identificamos que comando es si cd, umask, time o read
           			if (strcmp(argv[0], "cd") == 0) {
                		funcion_cd(argv, filev); //ejecuta la funcion de cd ya que ha leido el cd
                			
            		}else if(strcmp(argv[0], "umask") == 0){
						funcion_umask(argv,filev);//se ejecuta la funcion de umask
							
					}else if(strcmp(argv[0], "time")==0 ){
						funcion_time(argv,filev);//se ejecuta la funcion time
							
					}else if(strcmp(argv[0], "read")==0 ){
						funcion_read(argv,filev);//se ejecuta la funcion read
							
					}
                    actualizar_variables_especiales();//actualiza las variables especiales
					dup2(saved_stdin, STDIN_FILENO);//devuelve la entrada estandar 
                    close(saved_stdin); // Liberar el descriptor duplicado
					break;
        		}else{
                    if (argvc < tamtotal - 1) { // No crear pipe para el último comando
                	   pipe(fd);
            	    }
				    pid_t pid =fork();//Creamos un nuevo proceso hijo
		
				    if(pid == 0 ){//bloque del hijo
					    if (argvc == 0 && filev[0]) { // Redirección de entrada solo para el primer comando
        				    int fd_in = open(filev[0], O_RDONLY);//abrimos el archivo de entrada con open
        				    if (fd_in < 0) {//comprobamos si se ha habierto correctamente
          				        perror("Error abriendo archivo de entrada");
            				    exit(1);
        				    }
        				    dup2(fd_in, STDIN_FILENO);//redirecion de entrada estandar al archivo
        				    close(fd_in);//cerramos el descriptor del archivo despues de ser redirecionado
    				    }
					    if (argvc == tamtotal - 1 && filev[1]) { // Redirección de salida para el último comando
 					   	    int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);//abrimos o creamos el archivo de salida, con el open
   	 					    if (fd_out < 0) {//en caso de qeu no se halla abierto correctamente  que salte error
        					    perror("Error abriendo archivo de salida");
        					    exit(1);
    					    }
    					    dup2(fd_out, STDOUT_FILENO);//redirigimos la salida estandar al archivo
    					    close(fd_out); // cerramos el descriptor del archivo de salida
					    }
					    if (argvc > 0) { // Si no es el primer comando, redirigir entrada
                            //redirigimos la entrada del pipe anterior
                    	    dup2(fd_prev[0], 0);
                    	    close(fd_prev[0]);
						    close(fd_prev[1]);
                    	
               	 	    }	

                	    if (argvc < tamtotal - 1) { // Si no es el último comando, redirigir salida
                            //redirige la salida estandar al extremo de de escritura del pipe
                    	    dup2(fd[1], 1);
                    	    close(fd[0]);
                    	    close(fd[1]);
                	    }
					
                        /*ejecucion de comandos he distingido dos modelos de ejecucion, ejecucion is es un codigo interno y
                        si es un comando externo*/
					    if(esinterna(argv)==1 && argvc != tamtotal-1){

                            /*Ejecutamos los comandos internos guardando su status y actualizamos el status con la funcion de 
                            vaiables especiasle
                            */
						    if (strcmp(argv[0], "cd") == 0) {
                			    status=funcion_cd(argv, filev); 
                                actualizar_variables_especiales();
            			    }else if(strcmp(argv[0], "umask") == 0){
							    status=funcion_umask(argv,filev);
                                actualizar_variables_especiales();
						    }else if(strcmp(argv[0], "time")==0 ){
							    status=funcion_time(argv,filev);
                                actualizar_variables_especiales();
						    }else if(strcmp(argv[0], "read")==0 ){
                            
							    status=funcion_read(argv,filev);
                                actualizar_variables_especiales();
						    }
					    }else {
                            execvp(argv[0], argv); // Ejecutar el comando externo
            		        perror("Error ejecutando comando");
            		        exit(1);
                        }
					
				    }else if(pid>0){
                        /*Entramso al proceso padre que se encargara de almacenar el ultimo proceso hijo ejecutado
                        y cerrar las tuberias anteriores y guardar la tubería actual*/
					    if (argvc > 0) { // Cerrar la tubería anterior
                    	    close(fd_prev[0]);
                    	    close(fd_prev[1]);
                	    }
					    if (argvc < tamtotal - 1) { // Guardar la tubería actual como la anterior
                    	    fd_prev[0] = fd[0];
                    	    fd_prev[1] = fd[1];
                	    }
					    last_pid=pid;//guarda el pid del ultimo proceso hijo

				    }else{
                        //si ha fallado el fork entra aqui
					    perror("Fallo el fork");
        			    exit(-1);
				    }
				    actualizar_variables_especiales(); // Actualiza las variables especiales
                }
				

			}else{
                /*En caso de que solo haya un comando es muy sencillo ejecuta las funciones internas,
                en caso de que sea interno y no crea procesos hijos. En caso de que sean externos se crea un hijo y se ejecuta el comando
                */
				expandir_variables_especiales(argv);
				if(strcmp(argv[0], "read") == 0){
					status = funcion_read(argv,filev);
                    actualizar_variables_especiales(); 
				}else if(strcmp(argv[0], "time") == 0){
					status= funcion_time(argv,filev);
                    actualizar_variables_especiales(); 
				}else if(strcmp(argv[0], "umask") == 0){
					status=funcion_umask(argv,filev);
					actualizar_variables_especiales(); 
				}else if (strcmp(argv[0], "cd") == 0) {
						status=funcion_cd(argv, filev);
						actualizar_variables_especiales();  // Evita crear un pipeline para el comando interno
				}else{
					pid_t pid = fork(); // Crear un proceso hijo
            		if (pid == 0) {     // Proceso hijo
						if (bg == 0) {
        					// Si es un proceso en foreground, restaurar señales
        					restaurasignal();
    					} else {
        					// Si es un proceso en background, ignorar señales
        					ignorarsignal();
   						}
            			if (redirecionamiento(filev) < 0) {
                			exit(1); // Error en redirecciones
            			}
							execvp(argv[0], argv); // Ejecutar el comando
            				perror("Error ejecutando comando");
            				exit(1);
							
        			} else if (pid > 0) {  // Proceso padre
						
						if(bg==0){
							int child_status;
							waitpid(pid,&child_status,0); //esto nos garantiza la espera correcta a que acabe el hijo
							if (WIFEXITED(child_status)) {
								status = WEXITSTATUS(child_status);//captura el estado de salida
            					printf("Process %d exited with status %d\n", pid, WEXITSTATUS(child_status));
        					} else if (WIFSIGNALED(child_status)) {
								status = 128 + WTERMSIG(child_status);
            					printf("Process %d was terminated by signal %d\n", pid, WTERMSIG(child_status));//Termina por una señal
        					}
							actualizar_variables_especiales(); // Actualiza las variables especiales
						}else {
        					// Mostrar que el proceso se ejecuta en background
        					printf("Process %d running in background\n", pid);
							bgpid = pid; // Actualiza el valor de bgpid
    						actualizar_variables_especiales(); 
    					}
						
					}
					
				}

				
			}
			
		}
		if(tamtotal>1){
			int child_status;
			waitpid(last_pid, &child_status, 0);//Nos garantiza la espera al ultimo proces hijo cuando hay varios comandos
			if (WIFEXITED(child_status)) {
					status = WEXITSTATUS(child_status); // Captura el estado de salida del proceso hijo
        			printf("Process %d exited with status %d\n", last_pid, WEXITSTATUS(child_status));
        		} else if (WIFSIGNALED(child_status)) {
            		printf("Process %d was terminated by signal %d\n", last_pid, WTERMSIG(child_status));
					status = 128 + WTERMSIG(child_status); // Si terminó por una señal
        	}
    		actualizar_variables_especiales(); // Actualiza las variables especiales
		}
	}

	exit(0);
	return 0;
}


#if 0
/*
 * LAS LINEAS QUE A CONTINUACION SE PRESENTAN SON SOLO
 * PARA DAR UNA IDEA DE COMO UTILIZAR LAS ESTRUCTURAS
 * argvv Y filev. ESTAS LINEAS DEBERAN SER ELIMINADAS.
 */
		for (argvc = 0; (argv = argvv[argvc]); argvc++) {
			for (argc = 0; argv[argc]; argc++)
				printf("%s ", argv[argc]);
			printf("\n");
		}
		if (filev[0]) printf("< %s\n", filev[0]);/* IN */
		if (filev[1]) printf("> %s\n", filev[1]);/* OUT */
		if (filev[2]) printf(">& %s\n", filev[2]);/* ERR */
		if (bg) printf("&\n");
/*
 * FIN DE LA PARTE A ELIMINAR
 */
#endif

