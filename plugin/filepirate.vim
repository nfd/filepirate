python3 <<EOF
import sys
import os
import vim

plugin_dir = os.path.dirname(vim.eval('expand("<sfile>")'))
sys.path.insert(0, plugin_dir)
from vimfilepirate import filepirate_open, filepirate_key, filepirate_callback, filepirate_accept, filepirate_cancel, filepirate_up, filepirate_down, filepirate_bs, filepirate_rescan, filepirate_enter_insert_mode, filepirate_enter_normal_mode, filepirate_add_negative_filter
EOF

if !exists("g:filepirate_map_leader") || g:filepirate_map_leader != 0
	noremap <Leader>t :python3 filepirate_open()<CR>
endif

