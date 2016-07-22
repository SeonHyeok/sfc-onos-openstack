# OPENSTACK and ONOS Integration

## Install OVS with NSH patch

```
chmod +x start-ovs-sh.deb
./start-ovs-sh.deb
```

## Install ONOS

Read the onos_install.sh and run it.

### Install below features on ONOS

```
feature:install onos-openflow
feature:install onos-openflow-base
feature:install onos-ovsdatabase
feature:install onos-ovsdb-base
feature:install onos-drivers-ovsdb
feature:install onos-ovsdb-provider-host
feature:install onos-app-vtn-onosfw
externalportname-set -n onos_port2
```

## Install OpenStack

```
git clone https://git.openstack.org/openstack-dev/devstack -b stable/mitaka
```

### configure Openstack

Copy the local.conf file to the devstack directory.

```
cp local.conf devstack/local.conf
```

### Remove existing openstack code for Fresh installation
```
rm -rf /opt/stack
```

### start Devstack
If there are probolems with mySQL or rabbit restart the PC and unstack three times.

```
./unstack.sh ;./clean.sh; ./stack.sh
```

## Modify for ONOS integration

```
git clone https://github.com/openstack/networking-onos.git
cd networking-onos
sudo python setup.py install
```

### ONOS credentials
Copy conf_onos.ini from ~/.../networking_onos/etc to /etc/neutron/plugins/ml2/ and modify /etc/neutron/plugins/ml2/conf_onos.ini with appropriate url, username and password

Url: http://127.0.0.1:8181/onos/vtn
vim 
user: karaf
password: karaf

### networking-onos setup

in /etc/neutron/plugins/ml2/ml2.conf replace

```
mechanism_drivers = ...
```

with

```
mechanism_drivers = onos_ml2
```

in /opt/stack/neutron/neutron*.egg-info/entry_points.txt

```
[neutron.ml2.mechanism_drivers]
...
onos_ml2 = networking_onos.plugins.ml2.driver:ONOSMechanismDriver

[neutron.service_plugins]
...
onos_router = networking_onos.plugins.l3.driver:ONOSL3Plugin
```

using sed


### for DNS

in /etc/neutron/dhcp_agent.ini

```
dnsmasq_dns_servers = 8.8.8.8, 8.8.4.4
```
restart q-dhcp

### with sed 

```
sed -i 's/mechanism_drivers =.*/mechanism_drivers = onos_ml2/g' /etc/neutron/plugins/ml2/ml2_conf.ini
sed -i '/\[neutron.ml2.mechanism_drivers\]/a onos_ml2 = networking_onos.plugins.ml2.driver:ONOSMechanismDriver' /opt/stack/neutron/neutron*.egg-info/entry_points.txt
sed -i '/\[neutron.service_plugins\]/a onos_router = networking_onos.plugins.l3.driver:ONOSL3Plugin' /opt/stack/neutron/neutron*.egg-info/entry_points.txt
sed -i 's/#dnsmasq_dns_servers =.*/dnsmasq_dns_servers = 8.8.8.8, 8.8.4.4/g' /etc/neutron/dhcp_agent.ini
sed -i 's/cpu_mode =.*/cpu_mode = host-model/g' /etc/nova/nova.conf
```

### for internet connectivity
```
sudo sysctl net.ipv4.ip_forward=1 
sudo iptables -A FORWARD -d 172.24.4.0/24 -j ACCEPT 
sudo iptables -A FORWARD -s 172.24.4.0/24 -j ACCEPT 
sudo iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE 
```

### restart Neutron

```
screen -x -r stack
```

ctrl+a+n until q-svc. restart all processes from q-svc to n-cpu.

start q-svc with

```
/usr/local/bin/neutron-server \
    --config-file /etc/neutron/neutron.conf \
    --config-file /etc/neutron/plugins/ml2/ml2_conf.ini \
    --config-file /etc/neutron/plugins/ml2/conf_onos.ini \
    & echo $! >/opt/stack/status/stack/q-svc.pid; fg || echo "q-svc failed to start" | tee "/opt/stack/status/stack/q-svc.failure"
```



### enable commandline
```
source openrc admin
```

### verify neutron is running
```
neutron net-list
```

### Test integration, create network

```
neutron net-create sfcNet
neutron subnet-create sfcNet 10.1.0.0/24 --name sfcSubNet
neutron router-create router2 
neutron router-interface-add router2 sfcSubNet
```

verify that network was created:
http://<ip_onos>:8181/onos/vtn/networks

### create image

```
glance image-create --name ubuntu --disk-format qcow2 --container-format bare --file 
```

### create ports
```
neutron port-create --name p1 sfcNet
neutron port-create --name p2 sfcNet
neutron port-create --name p3 sfcNet
neutron port-create --name p4 sfcNet
neutron port-create --name p5 sfcNet
neutron port-create --name p6 sfcNet

neutron port-update p1 --no-security-groups
neutron port-update p2 --no-security-groups
neutron port-update p3 --no-security-groups
neutron port-update p4 --no-security-groups
neutron port-update p5 --no-security-groups
neutron port-update p6 --no-security-groups

neutron port-update p1 --port-security-enabled=False
neutron port-update p2 --port-security-enabled=False
neutron port-update p3 --port-security-enabled=False
neutron port-update p4 --port-security-enabled=False
neutron port-update p5 --port-security-enabled=False
neutron port-update p6 --port-security-enabled=False
```

### boot vms, generate keypair
Remove old keypairs and clear known hosts, create new keypair and boot instances.

```
#nova keypair-delete osKey
rm ~/.ssh/known_hosts # think twice about this!
rm osKey.pem
nova keypair-add osKey > osKey.pem
chmod 600 osKey.pem

openstack flavor create --ram 2048 --disk 15 --vcpus 2 sf.small

nova boot --image ubuntu --flavor m1.small --nic net-name=private  --nic port-id=$(neutron port-list |grep p1 |awk '{print $2}') --key-name osKey SRC
nova boot --image ubuntu --flavor sf.small --nic net-name=private  --nic port-id=$(neutron port-list |grep p2 |awk '{print $2}') --nic port-id=$(neutron port-list |grep p3 |awk '{print $2}') --key-name osKey SF1
nova boot --image ubuntu --flavor sf.small --nic net-name=private --nic port-id=$(neutron port-list |grep p4 |awk '{print $2}') --nic port-id=$(neutron port-list |grep p5 |awk '{print $2}') --key-name osKey SF2
nova boot --image ubuntu --flavor m1.small --nic net-name=private --nic port-id=$(neutron port-list |grep p6 |awk '{print $2}') --key-name osKey DST
```

### configure public ports, change IP addresses to IPs in private network
remove all security from ports in private network. They will become reachable from outside.

```
neutron port-update $(neutron port-list |grep 10.0.0.10 |awk '{print $2}') --no-security-groups 
neutron port-update $(neutron port-list |grep 10.0.0.12 |awk '{print $2}') --no-security-groups 
neutron port-update $(neutron port-list |grep 10.0.0.7 |awk '{print $2}') --no-security-groups 
neutron port-update $(neutron port-list |grep 10.0.0.6 |awk '{print $2}') --no-security-groups 

neutron port-update $(neutron port-list |grep 10.0.0.10 |awk '{print $2}') --port-security-enabled=False 
neutron port-update $(neutron port-list |grep 10.0.0.12 |awk '{print $2}') --port-security-enabled=False 
neutron port-update $(neutron port-list |grep 10.0.0.7 |awk '{print $2}') --port-security-enabled=False 
neutron port-update $(neutron port-list |grep 10.0.0.6 |awk '{print $2}') --port-security-enabled=False 
```

### Floating Address
you should be able to reach all vms with ssh. Perharps wait a minute or two (or 10...).

```
nova floating-ip-create public
nova floating-ip-create public
nova floating-ip-create public
nova floating-ip-create public

nova floating-ip-associate SRC $(nova floating-ip-list |grep 'public' | sed -n '1p'| awk '{print $4}')
nova floating-ip-associate SF1 $(nova floating-ip-list |grep 'public' | sed -n '2p'| awk '{print $4}')
nova floating-ip-associate SF2 $(nova floating-ip-list |grep 'public' | sed -n '3p'| awk '{print $4}')
nova floating-ip-associate DST $(nova floating-ip-list |grep 'public' | sed -n '4p'| awk '{print $4}')
```

### ssh into VMs

```
ssh -i osKey.pem ubuntu@$(nova floating-ip-list |grep 'public' | sed -n '1p'| awk '{print $4}')
ssh -i osKey.pem ubuntu@$(nova floating-ip-list |grep 'public' | sed -n '2p'| awk '{print $4}')
ssh -i osKey.pem ubuntu@$(nova floating-ip-list |grep 'public' | sed -n '3p'| awk '{print $4}')
ssh -i osKey.pem ubuntu@$(nova floating-ip-list |grep 'public' | sed -n '4p'| awk '{print $4}')
```
### Setup NICs
setup the interfaces in the vms (src, dst, sf1, sf2)

```
sudo ip link set dev eth1 down
sudo dhclient eth1
```

only in sf1 und sf2

```
sudo ip link set dev eth2 down
sudo dhclient eth2
```

## setup the sfc

### port pairs, pair group, Flow classifier, port chain
```
neutron port-pair-create PP1 --ingress p2 --egress p3
neutron port-pair-create PP2 --ingress p4 --egress p5
neutron port-pair-group-create --port-pair PP1 PPG1
neutron port-pair-group-create --port-pair PP2 PPG2
neutron flow-classifier-create --source-ip-prefix 10.1.0.3/24 --destination-ip-prefix 10.1.0.8/24 --logical-source-port P1 --logical-destination-port P6 FC1
neutron port-chain-create --port-pair-group PPG1 --port-pair-group PPG2 --flow-classifier FC1 PC1
```

--chain-parameters correlation=NSH

### delete port chain

```
neutron port-chain-delete PC1
neutron flow-classifier-delete FC1
neutron port-pair-group-delete PPG1
neutron port-pair-group-delete PPG2
neutron port-pair-delete PP1
neutron port-pair-delete PP2
```