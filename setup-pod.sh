podman pod create -p 3000:3000 rocketchat
podman run -dt --pod rocketchat --name mongo docker.io/library/mongo:5.0 --oplogSize 128 --replSet rs01
sleep 2
podman exec mongo mongo --eval "printjson(rs.initiate())"
podman run -dt --pod rocketchat --name rocket.chat -e PORT=3000 -e ROOT_URL=http://localhost:3000 -e 'MONGO_URL=mongodb://mongo:27017/rocketchat?replicaSet=rs01' -e 'MONGO_OPLOG_URL=mongodb://mongo:27017/local?replicaSet=rs01' docker.io/library/rocket.chat:6
