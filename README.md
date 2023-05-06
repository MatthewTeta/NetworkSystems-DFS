# Distributed File System Written in C
- *Author*: Matthew Teta (matthew.teta@colorado.edu)
- *Date*: 05/06/2023

## Usage:
1. Create a configuration file:  
    ```
    $ touch ~/dfc.conf
    ```  
    The file contents should be in the following format:  
    There is one line for each of the file servers:
    ```
    server dfs1 127.0.0.1:10001
    server dfs2 127.0.0.1:10002
    server dfs3 127.0.0.1:10003
    server dfs4 127.0.0.1:10004
    ```
2. Run the servers with the following usage:
    ```
    ./dfs ./dfs1 10001
    ./dfs ./dfs2 10002
    ./dfs ./dfs3 10003
    ./dfs ./dfs4 10004
    ```
3. Run the client with the following command:
    ```
    ./dfc <command> [filename] ... [filename]
    ```
    Command can be one of the following:
    - **list**, **get**, **put**

## Building from Source:
1. Clone the Respository
2. Run ```make```
   - This will build boths the client (```./dfc```) and the server (```./dfs```)

## Implementation Details:
Both the servers and the clients will be stateless (except for the files residing on each end host).  
- **list**: The dfc reaches out to each of the clients and asks for the list of file chunks. The available servers will each respond with the contents of each of the manifest files as well as the list of all chunk files available for reading.  
  The client will be responsible for determining if each of the files can be reconstructed based on the file manifests and the available file lists.
- **get**: The dfc first runs the **list** command to determine if the requested file exists and can be reconstructed from the available servers. It will then simply download each of the chunks from the available servers and reconstruct the file by moving the chunks into the destination. The file will be checked against the fiel checksum from the manifest.
- **put**:  
    - The dfc will first construct a manifest for the file to be distributed containing the following:
        - The original file name
        - The file checksum
        - The file size & number of chunks
        - The file modification timestamp
        - The names of each of the file chunks  
  - The file will be split into chunks of a fixed size as defined in the ```protocol.h``` file.  
  - The dfc will contact each of the dfs servers to determine if there is enough servers to distribute the file with the specified redundency (4 servers). If this is not the case, the client will return with an error.  
  - The chunks will be distributed to the dfs servers using the following scheme:  
    - Each chunk will be stored on a minimum of two servers. The ```file name hash + chunk #``` % ```dfs id``` is used to determine the placements.
