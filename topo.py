#!/usr/bin/python

from mininet.net import Mininet
from mininet.node import OVSController # 显式导入
from mininet.cli import CLI
from mininet.log import setLogLevel, info
from mininet.link import TCLink

def run():
    setLogLevel('info')
    # 使用显式指定的控制器类
    net = Mininet(controller=OVSController, link=TCLink)
    
    info('*** Adding controller\n')
    net.addController('c0')
    
    info('*** Adding hosts\n')
    h1 = net.addHost('h1', ip='10.0.0.1')
    h2 = net.addHost('h2', ip='10.0.0.2')
    
    info('*** Adding switch\n')
    s1 = net.addSwitch('s1')
    
    info('*** Creating links (10Mbps, 10ms delay)\n')
    # 修改 topo.py 里的 addLink
    net.addLink(h1, s1, bw=10, delay='10ms', loss=20) # 增加 5% 丢包
    net.addLink(h2, s1, bw=10, delay='10ms')
    
    net.start()
    
    # 禁用硬件校验和（Raw Socket 测试的关键）
    info('*** Disabling Checksum Offloading\n')
    # 注意：网卡名在 Mininet 中通常是 h1-eth0
    h1.cmd('ethtool -K h1-eth0 rx off tx off')
    h2.cmd('ethtool -K h2-eth0 rx off tx off')
        
    info('*** Configuring Firewall on h1\n')
    h1.cmd('iptables -A OUTPUT -p tcp --sport 12345 --tcp-flags RST RST -j DROP')
    
    CLI(net)
    net.stop()

if __name__ == '__main__':
    run()