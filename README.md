# AI智能体通信


# 编译
```bash
sudo apt install libhiredis-dev sqlite3

pip install --upgrade pip
pip install "urllib3<2.0" "chardet<5.0"
pip install cocan

conan profile detect # 创建默认配置文件
source ~/.profile  # 自己调整 并行构建线程数
# [conf]
# tools.build:jobs=2
source ~/.profile

mkdir build && cd build
conan install ..
conan install .. --build=missing # 有些依赖需要本地构建
```

# 技术实现进度
- ✅ 协程网络库
- ✅ Rock RPC （自定义二进制协议）
- ⬜ dpdk 实现用户态 TCP/IP 协议栈
- ⬜ ebpf 监控
- ⬜ MCP Server
- ⬜ Agent 智能体协调 
- ❎ quic 暂时搁置，在 dev_quic 分支

# 参考项目
- [sylar](https://github.com/sylar-yin/sylar)
- [quic-fiber](https://github.com/hankai17/quic-fiber)