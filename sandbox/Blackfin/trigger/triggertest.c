#include <stdio.h>
#include "trigger.h"
#include "dataset.h"



int main()
{
    testDataset();
}



int testDataset()
{
    struct dataset * s = createDataset(10000);
    if(s == NULL)
    {
        printf("Crap\n");
        return -1;
    }

    signed short cs = 0;

    int i=0, j=0;

    for(i=0; i<10000; i++)
        for(j=0; j<4; j++)
        {
            putSample(s, j, i, cs);
            cs++;
        }
    cs = 0;

    for(i=0; i<10000; i++)
        for(j=0; j<4; j++)
        {
            if(getSample(s, j, i) != cs)
                printf("fail: got %d wanted %d\n", getSample(s, j, i), cs);

            cs++;
        }
    destroyDataset(s);
}
