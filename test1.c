#include <stdlib.h>
#include <stdio.h>

int main()
{
  printf("Running test 1 to test a simple malloc and free\n");

    char * ptr = ( char * ) calloc ( 3, 3000 );
    printf("TestD\n");
    printf("%c\n", ptr[0]);
    printf("TestE\n");
    ptr[4064] = 'z';
    /*for(int i = 0; i < 4065; i++)
    {
        printf("%c", ptr[i]);
    }*/
    
  free( ptr ); 

  return 0;
}
