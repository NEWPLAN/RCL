requires:
	limited MAX_DATA_INFLIGHT:256, defualt is 128
			BUFFER_SIZE: 65535, default is 16*1024
			WINDOWS_NUM: 256, default is 256
			BATCH_MSG: 255, default is 64
state: 
	batch send and batch received, limited by MAX_DATA_INFLIGHT
	using buffer&msg in continuous memory space.
	confirm message back in batch, limited by BATCH_MSG
	Slicing window support, the max memory is expanded to 
		MAX_DATA_INFLIGHT*WINDOWS_NUM*BUFFER_SIZE

bug report: 
	not found yet

TBA:
	slacing window.
# run:
在每一个full-mesh-benchmark中创建一个data目录
将该benchmark.py放到data里面
在16个节点一起运行：python3 benchmark.py
