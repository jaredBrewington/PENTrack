import numpy as np
import pandas as pd

df = pd.read_csv('perfTransField.txt', sep='\s+', comment='%', names=['x','y','z','Bx','By','Bz'])

df = df.fillna(0)

df.to_csv('perfTransField_0.txt', index=False, header=False)