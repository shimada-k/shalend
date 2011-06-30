#!/bin/sh

KERNEL_TREE=/home/shimada/linux-source-2.6.32/
SHALEND_WD=/home/shimada/shalend/
CPUINFO=/proc/cpuinfo
OUTPUT_CSV="$SHALEND_WD"output.csv
OUTPUT_LB="$SHALEND_WD"output.lb
DATA_DIR="$SHALEND_WD"data.`date +%y.%m.%d`/


# -t '1つのランキューあたりのタスク数'

# 引数解析のサブシェルが一時ファイルを作るので、ファイルのあるなしで条件分岐
####

shalen_init()
{
# sarのためにロケールをCに変更
	LANG=C

	NR_CPU=`cat "$CPUINFO" | grep processor | wc -l`
# shalendを起動
	chmod +x "$SHALEND_WD"shalend
	"$SHALEND_WD"shalend workdir="$SHALEND_WD"
}

shalen_final()
{
# コンパイルの方が先に終了したら、shalendを停止
	if [ -e "$SHALEND_WD"shalend.pid ]
	then
		kill -15 `cat "$SHALEND_WD"shalend.pid`
	fi

#shalendが完全に停止するまで待つ
	sleep 10

	cd "$SHALEND_WD"

	./analb

#データファイルをディレクトリに移動
	mv "$OUTPUT_CSV" "$DATA_DIR""$1"FB_"$ELAPSE".csv
	mv "$OUTPUT_LB" "$DATA_DIR""$1"FB_"$ELAPSE".lb

#sarファイルから時間範囲指定してテキストファイルに出力する
	sar -A -s "$2" -e `date "+%T"` -f "$DATA_DIR"output.sar > "$DATA_DIR""$1"TASK_["$ELAPSE"].txt

#次回のために掃除
	cd "$KERNEL_TREE"
	make clean
}

#引数は1CPUあたりのタスク数
shalen_exec()
{
	shalen_init

	BUILD_T=`date "+%s"`
	NR_TASKS=`expr "$NR_CPU" \* "$1"`

# debug
####
	echo "$NR_CPU"
	echo "$NR_TASKS"

	START_T=`date "+%T"`

	cd "$KERNEL_TREE"
	make -j"$NR_TASKS"
	BUILT_T=`date "+%s"`
	ELAPSE=`expr "$BUILT_T" - "$BUILD_T"`

	shalen_final "$1" "$START_T"
}

mkdir "$DATA_DIR"

#sarで2秒ごとにシステム情報の取得開始（6400秒 == 3時間）
sar -P ALL 2 36000 -o "$DATA_DIR"output.sar > /dev/null &

#15から1づつseq(1)の第3引数までループ
for i in `seq 15 1 15`
do
    shalen_exec "$i"
done

