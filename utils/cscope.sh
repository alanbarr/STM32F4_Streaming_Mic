set -e

project_root=../
project_path1=${project_root}/stm32_streaming
project_path2=${project_root}/board_stm32f4_dis_bb
chibios_path=${project_root}/lib/ChibiOS
sys_path=/usr/lib/arm-none-eabi/

files_list=./cscope.files

# arguments: path, list
get_c_files () 
{
    find $1 -name "*.[chxsS]" >> $2
}

search_chibios ()
{
    exclude_strings=( \
    SPC \
    SIMIA32 \
    LPC \
    ST_STM32373C_EVAL \
    ST_STM3210C_EVAL \
    OLIMEX \
    MSP430 \
    ST_STM32L_DISCOVERY \
    MAPLEMINI_STM32_F103 \
    ST_STM3210E_EVAL \
    ST_STM32VL_DISCOVERY \
    ST_STM3220G_EVAL \
    ARMCM3-GENERIC-KERNEL \
    Posix \
    ARMCM4-SAM4L \
    AVR \
    ARMCM3-STM32L152-DISCOVERY \
    Win32 \
    AVR \
    PPC \
    AT91SAM7 \
    IAR \
    RVCT \
    ARDUINO \
    git \
    testhal \
    template \
    GPIOv1 \
    SPIv2)

    get_c_files $chibios_path $files_list

    for string in ${exclude_strings[@]}
    do 
        eval sed -i "/.*$string.*/d" $files_list 
        eval sed -i '/^$/d' $files_list
    done
}

search_sys ()
{
    get_c_files $sys_path $files_list
}

search_project ()
{
    get_c_files $project_path1 $files_list
    get_c_files $project_path2 $files_list
}

rm -f $files_list
search_project
search_chibios
search_sys
cscope -b -k
rm -f $files_list


