# Begin 4 servers with their stdouts redirected to files

# Get .sh file directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Start 4 servers
$DIR/../dfs $DIR/dfs1 10001 > $DIR/dfs1.log &
# $DIR/../dfs $DIR/dfs2 10002 > $DIR/dfs2.log &
# $DIR/../dfs $DIR/dfs3 10003 > $DIR/dfs3.log &
# $DIR/../dfs $DIR/dfs4 10004 > $DIR/dfs4.log &
