#include "validaciones.h"
#include "usuario.h"

// Validar contraseña
int validar_password(const char* pass)
{
    int charespc = 0;
    int num = 0;
    int mayus = 0;
    int minus = 0;
    int longitud = 0;

    for(int i = 0; pass[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        longitud++;
        if(pass[i] >= '0' && pass[i] <= '9') num++;
        else if(pass[i] >= 'A' && pass[i] <= 'Z') mayus++;
        else if(pass[i] >= 'a' && pass[i] <= 'z') minus++;
        else if(pass[i] == '*' || pass[i] == '/' || pass[i] == '-' ||
                pass[i] == '_' || pass[i] == '+' || pass[i] == '.') charespc++;
    }
    
    return (charespc >= 2 && num > 0 && mayus > 0 && minus > 0 && longitud >= 8) ? 0 : 1;
}
