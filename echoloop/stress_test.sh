rm -f build/out.txt
printf "Running 300 echoloops...\n"
for((i = 0; i < 300; i++))
do
	./build/echoloop "$i" >>build/out.txt &
done
sleep 1.5
printf "Remaining echoloops: (pid)\n"
pgrep echoloop
printf "Sening SIGQUIT:\n"
pkill -QUIT echoloop
printf "after:\n"
pgrep echoloop
printf "Out file:\n"
cat build/out.txt
