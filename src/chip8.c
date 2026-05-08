void chip8_init(Chip8 *chip8){

    memset(chip8,0,sizeof(Chip8)); //this sets the entire memory to 0

    chip8->pc = 0x200; // 0x200-0xFFF - Program ROM and work RAM

    chip8->I = 0;
    chip8->opcode = 0;

}

void chip8_load_rom(Chip8 *chip8 , const char *filename){

    FILE *file = fopen(filename, "rb"); //opens a file

    if (!file){
        printf("FAILED TO OPEN ROM \n");
        exit(1);
    }
    fseek(file,0, SEEK_END); //move cursor to end of file
    long size = ftell(file); //get the file size
    rewind(file); // now it returns to the beggining 

    fread(&chip->memory[0x200],size,1,file); //this reads rom to memmory

    fclose(file);
}