# RDMA Service
## 整体描述：
封装的RDMA service
***
## 文件说明：
- ```src```: 封装的动态链接库
  
- ```include```: include文件

- ```example```: 测试文件

## Compile：
- step1: 进入到子项目的$ROOT目录
- step2: 同步代码：```make sync``` 
  > 默认情况下， 代码主要同```12.12.12.111```进行同步，具体参考Makefile文件的同步逻辑
- step3: 编译代码：```make build```
- step4: 运行代码： ```./build/rdma_service_benchmark [para1] [para2], ..., [para]...```
  > 
  #example: 使用cluster模式运行

  ./build/rdma_service_benchmark --msg-size=400000  --cluster 12.12.12.111 12.12.12.112 12.12.12.113 12.12.12.114 --single-recv
  
## Usage：
benchmark提供了5种运行模式，分别是  ```server-client``` 模式, ```full-mesh``` 模式, ```ring``` 模式, ```tree``` 模式, ```double-binary-tree``` 模式.
- 参数解析
  - ```msg-size```: 指定每个message的大小（单位为byte），默认情况下是100K bytes
  - ```server```: 指定server IP，用于```server-client``` 模式。
  - ```num```：确定运行在```server-client``` 模式中receiver需要等待多少个connection requests
  - ```single-recv```：指定receiver的线程模式，如果运行在```server-client``` 模式，需要配合 ```num``` 参数确定receiver需要等待多少个connection requests
- 通信模式
  
  - ```server-client```模式：这种模式下，指定一个server节点，当前节点能够自我判断，如果是server节点，则会启用server-services，用于接受client发过来的数据。否则，启用client services，用于向server推送数据。
    - 在这种模式下，如果使用单线程模式接受数据，则需要明确指定 ```num``` 参数。
    - >  #使用例子：(每个物理节点都允许同样的命令)
    >
    > ./build/rdma_service_benchmark --msg-size=1000000 --num 3 --single-recv --server 12.12.12.114

  - ```full-mesh``` 模式：用于Full-Mesh的通信模式，每个节点既可以当作server用于接收多个连接的数据推送，同时也会启用client，用于数据推送
    >  #使用例子：(每个物理节点都允许同样的命令)
    >
    > ./build/rdma_service_benchmark --msg-size=400000 --topo full-mesh  --single-recv --cluster 12.12.12.111 12.12.12.112 12.12.12.113 12.12.12.114

  - ```ring``` 模式：用于ring的通信模式，每个节点既可以当作server用于接收1个连接的数据推送，同时也会启用1个client，用于数据推送
    >  #使用例子：(每个物理节点都允许同样的命令)
    >
    > ./build/rdma_service_benchmark --msg-size=400000 --topo ring --single-recv --cluster 12.12.12.111 12.12.12.112 12.12.12.113 12.12.12.114

  - ```tree``` 模式：用于tree的通信模式，每个节点既可以当作server用于接收多个连接的数据推送，同时也会启用client，用于数据推送。需要配合```tree-width```参数，指定通信宽度。
    >  #使用例子：(每个物理节点都允许同样的命令)
    >
    > ./build/rdma_service_benchmark --msg-size=400000 --topo tree  --tree-width=2  --single-recv --cluster 12.12.12.111 12.12.12.112 12.12.12.113 12.12.12.114

  - ```double-binary-tree``` 模式：用于double-binary-tree的通信模式，每个节点既可以当作server用于接收多个连接的数据推送，同时也会启用client，用于数据推送
    >  #使用例子：(每个物理节点都允许同样的命令)
    >
    > ./build/rdma_service_benchmark --msg-size=400000 --topo dbt  --single-recv --cluster 12.12.12.111 12.12.12.112 12.12.12.113 12.12.12.114

    启用更多的LOG信息：
    GLOG_v