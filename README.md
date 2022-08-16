# MaelstromDB
MaelstromDB is a toy implementation of a distributed SQL database using the Raft protocol.

## Raft Features
- [X] Leader election
- [X] Log replication
- [X] Cluster membership changes, dynamically add/remove servers to cluster
- [X] Linearizable semantics for clients
- [X] Group commits, to improve write throughput
- [X] Leader leases, to serve reads from Leader without consulting Followers to reduce read latency
- [ ] Snapshots, to prevent unbounded growth of log

## Prerequisites
The database was tested with the following versions of docker and docker-compose,
- docker >= 20.10.5
- docker-compose >= 1.29.0

## Usage
To create a 3 node cluster run,
```
docker-compose build
docker-compose up -d node1 node2 node3
docker-compose run admin
```
This should open a shell from which the cluster can be connected with,
```
./maelstromcli reconfigure --cluster=node1:3000 node1:3000,node2:3000,node3:3000
```
To write key-value pairs run,
```
./maelstromcli write --cluster=node1:3000,node2:3000,node3:3000 $key:$value
```
substituting $key and $value with the key value pair.
To read data run,
```
./maelstromcli query --cluster=node1:3000,node2:3000,node3:3000 $key
```
substituting $key with the known key.

