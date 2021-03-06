#import SimpleCV
import numpy as np
#import Scientific.IO.NetCDF as Dataset
import scipy.io.netcdf as Dataset
#from netCDF4 import Dataset
import cv2
import os


def createSampleImages(dataDir, trialName):
   
    # movie and images
    movieName = dataDir + "allVideos/" + trialName + ".MOV";
    bkGrnd = cv2.imread(dataDir + "backGrounds/bk-" + trialName + ".png")
    bkGrnd = cv2.cvtColor(bkGrnd, cv2.COLOR_BGR2GRAY)
    #mask = mask.createBinaryMask(color1=(1,1,1),color2=(255,255,255))

    # open netcdf file
    ncFileName = dataDir + "tracked/linked" + trialName + ".nc"    
    f = Dataset.NetCDFFile(ncFileName, 'a')
   

    # get the positions variable
    trXY = f.variables['trXY']
    trackList = []
    trackList = np.empty_like (trXY.data)
    np.copyto(trackList,trXY.data)

    # get the movie frame numbers
    frNum = f.variables['frNum']
    trFrames = []
    trFrames = np.empty_like(frNum.data)
    np.copyto(trFrames,frNum.data)

    # get the fish identities
    fid = f.variables['fid']
    fishIDs = -np.ones(np.shape(fid.data),'int16')
    
    # these variables store the index values when a track is present
    [trackIndex,timeIndex]=np.nonzero(trackList[:,:,0])

    # to find where tracks come and go we sum all the track IDs at each time point
    maxTime = np.size(trackList,1)
    trackSum = np.zeros([maxTime])
    trackCount = np.zeros([maxTime])
    for k in range(maxTime): 
        trackSum[k]=sum(trackIndex[timeIndex[:]==k])
        trackCount[k]=np.size(trackIndex[timeIndex[:]==k])

    # points where the sum of the track IDs change are when a track is lost or found
    d= np.diff(trackSum)
    # mark the start and end points
    d[0]=1
    d[-1]=1
    # now find where the tracks change
    idx, = d.nonzero()
    # because diff calculates f(n)-f(n+1) we need the next entry found for all except d[0]
    idx[idx>0]=idx[idx>0]+1
    # the difference between the indices gives the length of each block of continuous tracks
    conCounts =  idx[1:] - idx[0:-1]
   
    # array of all tracks || length of time tracks are present || number of tracks || start index || stop index || start frame of movie || stop frame of movie
    mainTrackList = np.column_stack((trackCount[idx[0:-1]], conCounts, idx[0:-1],idx[1:]))
    
    # now sort the array by order of length
    ind = np.lexsort((-mainTrackList[:,1],-mainTrackList[:,0]))
    mainTrackList = mainTrackList[ind,:].astype(int)

    numBlocks =  np.size(mainTrackList,0)

    # get the parameters for the longest track block 
    thisTrackCount = mainTrackList[0,0]
    indexStart = mainTrackList[0,2]

    liveTracks = trackIndex[timeIndex[:]==indexStart]
    
    # as there's a restriction on the difference in the number of samples we won't use all images for all tracks
    # libSVM throws an error if one track has more than twice the number of another
    startIndex = indexStart #np.min(timeIndex[np.in1d(trackIndex,liveTracks)])
    
    maxStop = startIndex
    minStop = np.max(timeIndex[np.in1d(trackIndex,liveTracks)])
    for tr in range(thisTrackCount):
        thisStop = np.max(timeIndex[np.in1d(trackIndex,liveTracks[tr])])
        if thisStop>maxStop:
            maxStop = thisStop
        elif thisStop<minStop:
            minStop = thisStop
    
    stopIndex = int(min(startIndex + 2*(minStop -startIndex),maxStop))


    


    # now go through and store images from each track in a separate folder
    box_dim = 50
    bd2 = int(box_dim*0.5)
    cap = cv2.VideoCapture(movieName)
    cap.set(cv2.CAP_PROP_POS_FRAMES,trFrames[startIndex])

#   cv2.NamedWindow("w1", cv2.WINDOW_AUTOSIZE)
    for fr in range(startIndex,stopIndex):
        ret, frame = cap.read()
        thisIm = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
#       cv2.imshow('frame',thisIm)
#        cv2.waitKey(0)
        thisIm = cv2.absdiff(thisIm, bkGrnd)
#        thisIm = thisIm.applyBinaryMask(mask)
        
        for tr in range(thisTrackCount):
            xp = int(trackList[liveTracks[tr], fr,0])
            yp = int(trackList[liveTracks[tr], fr,1])
#          print fr, xp, yp
            if xp>0:
                direct = dataDir + '/process/' + trialName + '/FR_ID' + str(tr)
                tmpImg = thisIm[int(round(yp))-bd2:int(round(yp))+bd2, int(round(xp))-bd2:int(round(xp))+bd2]
                if tmpImg.shape[0]==box_dim and tmpImg.shape[1]==box_dim:
                    save_path = direct + "/img-" + str(fr) + ".png"
                    cv2.imwrite(save_path, tmpImg)
            
    # store the IDs     
    for tr in range(thisTrackCount):
        fishIDs[liveTracks[tr],:] = tr
        
    np.copyto(fid.data,fishIDs)
    f.sync()
    f.close()
    
    return mainTrackList
    



