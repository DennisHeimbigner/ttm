BEGINFILE			{
				testpat1 = "^[ 	]*Start[ 	]+[0-9]+:[ 	]+" test ".ttm"
				testpat2 = "^[0-9]+/[0-9]+[ 	]+Test[ 	]+[#][0-9]+:[ 	]+" test ".ttm"
				if (DBG == 1) print ">>> " testpat
				del = 1
				intest = 0
				DBG=0
				}
$0 ~ testpat1			{
				if (DBG == 1) print "@@@ reg del=" del
				intest = 1
				}
/Begin startup commands/	{
				if (DBG == 1) print "@@@ bsc intest=" intest
				if (intest == 1) del = 0
				}
$0 ~ testpat2			{
				if (DBG == 1) print "@@@ eoo intest=" intest
				if (DBG == 1) print "@@@ eoo del=" del
				del = 1
				intest = 0
				}
/^.*$/				{
				if (DBG == 1) print "@@@ any intest=" intest
				if (DBG == 1) print "@@@ any del=" del
				if ( del == 0 ) {
				    sub(/^[0-9]:[ 	]+/, "")
				    print
				    }
				}
