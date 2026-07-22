def dbos161_dumbvm
  dir ../src/kern/compile/DUMBVM
  target remote unix:.sockets/gdb
end

def dbos161_shell
  dir ../src/kern/compile/SHELL
  target remote unix:.sockets/gdb
end

dbos161_shell