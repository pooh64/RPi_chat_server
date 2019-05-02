rm -f build/out.txt
for((i = 0; i < 301; i++))
do
	./build/echoloop "$i" >>build/out.txt &
done
sleep 1.5
pgrep echoloop
pkill echoloop
cat build/out.txt
