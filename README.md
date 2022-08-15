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
./maelstromcli write --cluster=node1:3000,node2:3000,node3:3000 ${key}:${value}
```
substituting ${key} and ${value} with the key value pair.
To read data run,
```
./maelstromcli query --cluster=node1:3000,node2:3000,node3:3000 ${key}
```
substituting ${key} with the known key.


