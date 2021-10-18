import numpy as np
import LUT
import time

def PROFILE(func, *args, **kwargs) : 
    t1 = time.time()
    ret = func(*args, **kwargs)
    t2 = time.time()
    print(func.__name__ , ' process time : ', t2-t1 , ' seconds')
    return ret

def load_volume(file_path) -> np.array :
    with open(file_path,'rb') as f :
        buf = f.read()
        data = np.frombuffer(buf, dtype=np.float32)
    return data

def padding_clamp_to_edge(data, x, y, z) : 
    ret = np.zeros(shape=(z+1,y+1,x+1), dtype=np.float32)
    #ret[1:, 1:, 1:] = data
    #ret[0:-1, 0:-1, 1:] = data
    #ret[0:-1, 1:, 0:-1] = data
    #ret[1:, 0:-1, 0:-1] = data
    #ret[0:-1, 0:-1, 0:-1] = data
    ret[:z, :y, :x] = data
    return ret

def edge_test(vol, x, y, z, isovalue) : 
    ret = np.zeros(shape=(z,y,x,3),dtype=np.int32)
    
    origin = vol[:z, :y, :x] > isovalue
    x_shift_vol = vol[:z, :y, 1:] > isovalue
    y_shift_vol = vol[:z, 1:, :x] > isovalue
    z_shift_vol = vol[1:, :y, :x] > isovalue
    
    ret[:,:,:,0] = origin ^ x_shift_vol
    ret[:,:,:,1] = origin ^ y_shift_vol
    ret[:,:,:,2] = origin ^ z_shift_vol

    print(ret.shape)

    return ret.flatten().astype(np.int32)

def cell_test(vol,  x, y, z, isovalue) : 
    cell_type = np.zeros(shape = (z,y,x), dtype = np.int32)
    tri_count = np.zeros(shape = (z,y,x), dtype = np.int32)
    origin = (vol > isovalue).astype(np.int32)
    cell_type[:,:,:] = origin[:z,:y,:x] | (origin[:z,:y,1:] << 1) |  (origin[:z, 1:, 1:] << 2) | (origin[:z, 1:, :x] << 3) | (origin[1:, : y, : x] << 4) | (origin[1:, :y, 1:] << 5 ) | (origin[1:, 1:, 1:] << 6) | (origin[1:, 1:, :x] << 7)
    for i in range(z) : 
        for j in range(y) : 
            for k in range(x) : 
                tri_count[i,j,k] = LUT.triangle[cell_type[i,j,k]]
    return (cell_type, tri_count)

def main() :
    vol = load_volume("data/dragon_vrip_FLT32_128_128_64.raw").reshape(64,128,128)
    #vol = np.zeros(shape=(64,128,128), dtype=np.float32)
    #vol[1,1,1] =1.0
    isovalue = 0.0
    dim = [128, 128, 64] 
    vol = padding_clamp_to_edge(vol, *dim)
    edge_type = PROFILE(edge_test, vol, 128, 128, 64, isovalue)
    cell_type, tri_count = PROFILE(cell_test, vol, 128, 128, 64, isovalue)
    print('edge_test result : ', np.sum(edge_type))
    print('tri_count result : ', np.sum(tri_count))

if __name__ =='__main__' :
    main()
