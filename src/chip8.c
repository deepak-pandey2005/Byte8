void chip8_init(Chip8 *chip8){

    memset(chip8,0,sizeof(Chip8)); //this sets the entire memory to 0

    chip8->pc = 0x200; // 0x200-0xFFF - Program ROM and work RAM

    chip8->I = 0;
    chip8->opcode = 0;

}

void chip8_load_application(Chip8 *chip8 , const char *filename){
    chip8_init(Chip8);
    printf("LOADING : %s \n", filename);
    FILE *file = fopen(filename, "rb"); //opens a file

    if (!file){
        printf("FAILED TO OPEN ROM \n");
        exit(1);
    }
    fseek(file,0, SEEK_END); //move cursor to end of file
    long size = ftell(file); //get the file size
    rewind(file); // now it returns to the beggining 

    //allocate memory to contain the whole file
    char * buffer = (char*)malloc(sizeof(char) * size);
    if (buffer == NULL){
        fputs("MEMORY ERROR",stderr);
        exit(1);
    }

    //copy the whole file into the buffer
    size_t result = fread (buffer,1,size,file);
    if(result != size){
        fputs("READING ERROR",stderr);
        exit(1);
    }

    //copy buffer to chip8 memory
    if(size <=(4096-512)){
        for(int i = 0 ; i<size ; i++)
           Chip8->memory[i+ 512] = buffer[i];
    }
    else
    {
        printf("ERROR ;ROM too big");
    }

    // close file ,free buffer
    fclose(file);
    free(buffer);
}