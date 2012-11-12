/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* rollback and rollforward routines. */

#include <ft/log_header.h>

#include "ft.h"
#include "ft-ops.h"
#include "log-internal.h"
#include "xids.h"
#include "rollback-apply.h"

// functionality provided by roll.c is exposed by an autogenerated
// header file, logheader.h
//
// this (poorly) explains the absense of "roll.h"

// these flags control whether or not we send commit messages for
// various operations

// When a transaction is committed, should we send a FT_COMMIT message
// for each FT_INSERT message sent earlier by the transaction?
#define TOKU_DO_COMMIT_CMD_INSERT 0

// When a transaction is committed, should we send a FT_COMMIT message
// for each FT_DELETE_ANY message sent earlier by the transaction?
#define TOKU_DO_COMMIT_CMD_DELETE 1

int
toku_commit_fdelete (FILENUM    filenum,
                     TOKUTXN    txn,
                     LSN        UU(oplsn)) //oplsn is the lsn of the commit
{
    int r;
    CACHEFILE cf;
    CACHETABLE ct = txn->logger->ct;

    // Try to get the cachefile for this filenum. A missing file on recovery
    // is not an error, but a missing file outside of recovery is.
    r = toku_cachefile_of_filenum(ct, filenum, &cf);
    if (r == ENOENT) {
        assert(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    assert_zero(r);

    // bug fix for #4718
    // bug was introduced in with fix for #3590
    // Before Maxwell (and fix for #3590), 
    // the recovery log was fsynced after the xcommit was loged but 
    // before we processed rollback entries and before we released
    // the row locks (in the lock tree). Due to performance concerns,
    // the fsync was moved to after the release of row locks, which comes
    // after processing rollback entries. As a result, we may be unlinking a file
    // here as part of a transactoin that may abort if we do not fsync the log.
    // So, we fsync the log here.
    if (txn->logger) {
        toku_logger_fsync_if_lsn_not_fsynced(txn->logger, txn->do_fsync_lsn);
    }

    // Mark the cachefile as unlink on close. There are two ways for close
    // to be eventually called on the cachefile:
    //
    // - when this txn completes, it will release a reference on the 
    // ft and close it, UNLESS it was pinned by checkpoint
    // - if the cf was pinned by checkpoint, an unpin will release the 
    // final reference and call close. it must be the final reference
    // since this txn has exclusive access to dictionary (by the
    // directory row lock for its dname) and we would not get this
    // far if there were other live handles.
    toku_cachefile_unlink_on_close(cf);
done:
    return r;
}

int
toku_rollback_fdelete (FILENUM    UU(filenum),
                       TOKUTXN    UU(txn),
                       LSN        UU(oplsn)) //oplsn is the lsn of the abort
{
    //Rolling back an fdelete is an no-op.
    return 0;
}

int
toku_commit_fcreate (FILENUM UU(filenum),
                     BYTESTRING UU(bs_fname),
                     TOKUTXN    UU(txn),
                     LSN        UU(oplsn))
{
    return 0;
}

int
toku_rollback_fcreate (FILENUM    filenum,
                       BYTESTRING UU(bs_fname),
                       TOKUTXN    txn,
                       LSN        UU(oplsn))
{
    int r;
    CACHEFILE cf;
    CACHETABLE ct = txn->logger->ct;

    // Try to get the cachefile for this filenum. A missing file on recovery
    // is not an error, but a missing file outside of recovery is.
    r = toku_cachefile_of_filenum(ct, filenum, &cf);
    if (r == ENOENT) {
        assert(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    assert_zero(r);

    // Mark the cachefile as unlink on close. There are two ways for close
    // to be eventually called on the cachefile:
    //
    // - when this txn completes, it will release a reference on the 
    // ft and close it, UNLESS it was pinned by checkpoint
    // - if the cf was pinned by checkpoint, an unpin will release the 
    // final reference and call close. it must be the final reference
    // since this txn has exclusive access to dictionary (by the
    // directory row lock for its dname) and we would not get this
    // far if there were other live handles.
    toku_cachefile_unlink_on_close(cf);
done:
    return 0;
}

int find_ft_from_filenum (const FT &h, const FILENUM &filenum);
int find_ft_from_filenum (const FT &h, const FILENUM &filenum) {
    FILENUM thisfnum = toku_cachefile_filenum(h->cf);
    if (thisfnum.fileid<filenum.fileid) return -1;
    if (thisfnum.fileid>filenum.fileid) return +1;
    return 0;
}

// Input arg reset_root_xid_that_created true means that this operation has changed the definition of this dictionary.
// (Example use is for schema change committed with txn that inserted cmdupdatebroadcast message.)
// The oplsn argument is ZERO_LSN for normal operation.  When this function is called for recovery, it has the LSN of
// the operation (insert, delete, update, etc).
static int do_insertion (enum ft_msg_type type, FILENUM filenum, BYTESTRING key, BYTESTRING *data, TOKUTXN txn, LSN oplsn,
                         bool reset_root_xid_that_created) {
    CACHEFILE cf;
    // 2954 - ignore messages for aborted hot-index
    int r = 0;
    //printf("%s:%d committing insert %s %s\n", __FILE__, __LINE__, key.data, data.data);
    r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    if (r==ENOENT) { //Missing file on recovered transaction is not an error
        assert(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    assert(r==0);

    FT h;
    h = NULL;
    r = txn->open_fts.find_zero<FILENUM, find_ft_from_filenum>(filenum, &h, NULL);
    assert(r==0);

    if (oplsn.lsn != 0) {  // if we are executing the recovery algorithm
        LSN treelsn = toku_ft_checkpoint_lsn(h);  
        if (oplsn.lsn <= treelsn.lsn) {  // if operation was already applied to tree ...
            r = 0;                       // ... do not apply it again.
            goto done;
        }
    }

    DBT key_dbt,data_dbt;
    XIDS xids;
    xids = toku_txn_get_xids(txn);
    {
        FT_MSG_S ftcmd = { type, ZERO_MSN, xids,
                           .u = { .id = { (key.len > 0)
                                          ? toku_fill_dbt(&key_dbt,  key.data,  key.len)
                                          : toku_init_dbt(&key_dbt),
                                          data
                                          ? toku_fill_dbt(&data_dbt, data->data, data->len)
                                          : toku_init_dbt(&data_dbt) } } };

        toku_ft_root_put_cmd(h, &ftcmd);
        if (reset_root_xid_that_created) {
            TXNID new_root_xid_that_created = xids_get_outermost_xid(xids);
            toku_reset_root_xid_that_created(h, new_root_xid_that_created);
        }
    }
done:
    return r;
}


static int do_nothing_with_filenum(TOKUTXN UU(txn), FILENUM UU(filenum)) {
    return 0;
}


int toku_commit_cmdinsert (FILENUM filenum, BYTESTRING UU(key), TOKUTXN txn, LSN UU(oplsn)) {
#if TOKU_DO_COMMIT_CMD_INSERT
    return do_insertion (FT_COMMIT_ANY, filenum, key, 0, txn, oplsn, false);
#else
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_rollback_cmdinsert (FILENUM    filenum,
                         BYTESTRING key,
                         TOKUTXN    txn,
                         LSN        oplsn)
{
    return do_insertion (FT_ABORT_ANY, filenum, key, 0, txn, oplsn, false);
}

int
toku_commit_cmdupdate(FILENUM    filenum,
                      BYTESTRING key,
                      TOKUTXN    txn,
                      LSN        oplsn)
{
    return do_insertion(FT_COMMIT_ANY, filenum, key, 0, txn, oplsn, false);
}

int
toku_rollback_cmdupdate(FILENUM    filenum,
                        BYTESTRING key,
                        TOKUTXN    txn,
                        LSN        oplsn)
{
    return do_insertion(FT_ABORT_ANY, filenum, key, 0, txn, oplsn, false);
}

int
toku_commit_cmdupdatebroadcast(FILENUM    filenum,
                               bool       is_resetting_op,
                               TOKUTXN    txn,
                               LSN        oplsn)
{
    // if is_resetting_op, reset root_xid_that_created in
    // relevant ft.
    bool reset_root_xid_that_created = (is_resetting_op ? true : false);
    const enum ft_msg_type msg_type = (is_resetting_op
                                        ? FT_COMMIT_BROADCAST_ALL
                                        : FT_COMMIT_BROADCAST_TXN);
    BYTESTRING nullkey = { 0, NULL };
    return do_insertion(msg_type, filenum, nullkey, 0, txn, oplsn, reset_root_xid_that_created);
}

int
toku_rollback_cmdupdatebroadcast(FILENUM    filenum,
                                 bool       UU(is_resetting_op),
                                 TOKUTXN    txn,
                                 LSN        oplsn)
{
    BYTESTRING nullkey = { 0, NULL };
    return do_insertion(FT_ABORT_BROADCAST_TXN, filenum, nullkey, 0, txn, oplsn, false);
}

int
toku_commit_cmddelete (FILENUM    filenum,
                       BYTESTRING key,
                       TOKUTXN    txn,
                       LSN        oplsn)
{
#if TOKU_DO_COMMIT_CMD_DELETE
    return do_insertion (FT_COMMIT_ANY, filenum, key, 0, txn, oplsn, false);
#else
    key = key; oplsn = oplsn;
    return do_nothing_with_filenum(txn, filenum);
#endif
}

int
toku_rollback_cmddelete (FILENUM    filenum,
                         BYTESTRING key,
                         TOKUTXN    txn,
                         LSN        oplsn)
{
    return do_insertion (FT_ABORT_ANY, filenum, key, 0, txn, oplsn, false);
}

static int
toku_apply_rollinclude (TXNID      xid,
                        uint64_t   num_nodes,
                        BLOCKNUM   spilled_head,
                        uint32_t   spilled_head_hash __attribute__((__unused__)),
                        BLOCKNUM   spilled_tail,
                        uint32_t   spilled_tail_hash,
                        TOKUTXN    txn,
                        LSN        oplsn,
                        apply_rollback_item func) {
    int r = 0;
    struct roll_entry *item;

    BLOCKNUM next_log      = spilled_tail;
    uint32_t next_log_hash = spilled_tail_hash;
    uint64_t last_sequence = num_nodes;

    bool found_head = false;
    assert(next_log.b != ROLLBACK_NONE.b);
    while (next_log.b != ROLLBACK_NONE.b) {
        //pin log
        ROLLBACK_LOG_NODE log;
        toku_get_and_pin_rollback_log(txn, next_log, next_log_hash, &log);
        toku_rollback_verify_contents(log, xid, last_sequence - 1);
        last_sequence = log->sequence;
        
        toku_maybe_prefetch_previous_rollback_log(txn, log);

        while ((item=log->newest_logentry)) {
            log->newest_logentry = item->prev;
            r = func(txn, item, oplsn);
            if (r!=0) return r;
        }
        if (next_log.b == spilled_head.b) {
            assert(!found_head);
            found_head = true;
            assert(log->sequence == 0);
        }
        next_log      = log->previous;
        next_log_hash = log->previous_hash;
        {
            //Clean up transaction structure to prevent
            //toku_txn_close from double-freeing
            spilled_tail      = next_log;
            spilled_tail_hash = next_log_hash;
            if (found_head) {
                assert(next_log.b == ROLLBACK_NONE.b);
                spilled_head      = next_log;
                spilled_head_hash = next_log_hash;
            }
        }
        toku_rollback_log_unpin_and_remove(txn, log);
    }
    return r;
}

int
toku_commit_rollinclude (TXNID      xid,
                         uint64_t   num_nodes,
                         BLOCKNUM   spilled_head,
                         uint32_t   spilled_head_hash,
                         BLOCKNUM   spilled_tail,
                         uint32_t   spilled_tail_hash,
                         TOKUTXN    txn,
                         LSN        oplsn) {
    int r;
    r = toku_apply_rollinclude(xid, num_nodes,
                               spilled_head, spilled_head_hash,
                               spilled_tail, spilled_tail_hash,
                               txn, oplsn,
                               toku_commit_rollback_item);
    return r;
}

int
toku_rollback_rollinclude (TXNID      xid,
                           uint64_t   num_nodes,
                           BLOCKNUM   spilled_head,
                           uint32_t   spilled_head_hash,
                           BLOCKNUM   spilled_tail,
                           uint32_t   spilled_tail_hash,
                           TOKUTXN    txn,
                           LSN        oplsn) {
    int r;
    r = toku_apply_rollinclude(xid, num_nodes,
                               spilled_head, spilled_head_hash,
                               spilled_tail, spilled_tail_hash,
                               txn, oplsn,
                               toku_abort_rollback_item);
    return r;
}

int
toku_commit_load (FILENUM    old_filenum,
                  BYTESTRING UU(new_iname),
                  TOKUTXN    txn,
                  LSN        UU(oplsn))
{
    int r;
    CACHEFILE old_cf;
    CACHETABLE ct = txn->logger->ct;

    // To commit a dictionary load, we delete the old file
    //
    // Try to get the cachefile for the old filenum. A missing file on recovery
    // is not an error, but a missing file outside of recovery is.
    r = toku_cachefile_of_filenum(ct, old_filenum, &old_cf);
    if (r == ENOENT) {
        invariant(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    lazy_assert(r == 0);

    // bug fix for #4718
    // bug was introduced in with fix for #3590
    // Before Maxwell (and fix for #3590), 
    // the recovery log was fsynced after the xcommit was loged but 
    // before we processed rollback entries and before we released
    // the row locks (in the lock tree). Due to performance concerns,
    // the fsync was moved to after the release of row locks, which comes
    // after processing rollback entries. As a result, we may be unlinking a file
    // here as part of a transactoin that may abort if we do not fsync the log.
    // So, we fsync the log here.
    if (txn->logger) {
        toku_logger_fsync_if_lsn_not_fsynced(txn->logger, txn->do_fsync_lsn);
    }

    // TODO: Zardosht
    // Explain why this condition is valid, because I forget.
    if (!toku_cachefile_is_unlink_on_close(old_cf)) {
        toku_cachefile_unlink_on_close(old_cf);
    }
done:
    return r;
}

int
toku_rollback_load (FILENUM    UU(old_filenum),
                    BYTESTRING new_iname,
                    TOKUTXN    txn,
                    LSN        UU(oplsn)) 
{
    int r;
    CACHEFILE new_cf;
    CACHETABLE ct = txn->logger->ct;

    // To rollback a dictionary load, we delete the new file.
    // Try to get the cachefile for the new fname.
    char *fname_in_env = fixup_fname(&new_iname);
    r = toku_cachefile_of_iname_in_env(ct, fname_in_env, &new_cf);
    if (r == ENOENT) {
        // It's possible the new iname was never created, so just try to 
        // unlink it if it's there and ignore the error if it's not.
        char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(ct, fname_in_env);
        r = unlink(fname_in_cwd);
        assert(r == 0 || get_error_errno() == ENOENT);
        toku_free(fname_in_cwd);
        r = 0;
    } else {
        assert_zero(r);
        toku_cachefile_unlink_on_close(new_cf);
    }
    toku_free(fname_in_env);
    return r;
}

//2954
int
toku_commit_hot_index (FILENUMS UU(hot_index_filenums),
                       TOKUTXN  UU(txn), 
                       LSN      UU(oplsn))
{
    // nothing
    return 0;
}

int
toku_rollback_hot_index (FILENUMS UU(hot_index_filenums),
                         TOKUTXN  UU(txn), 
                         LSN      UU(oplsn))
{
    return 0;
}

int
toku_commit_dictionary_redirect (FILENUM UU(old_filenum),
                                 FILENUM UU(new_filenum),
                                 TOKUTXN UU(txn),
                                 LSN     UU(oplsn)) //oplsn is the lsn of the commit
{
    //Redirect only has meaning during normal operation (NOT during recovery).
    if (!txn->recovered_from_checkpoint) {
        //NO-OP
    }
    return 0;
}

int
toku_rollback_dictionary_redirect (FILENUM old_filenum,
                                   FILENUM new_filenum,
                                   TOKUTXN txn,
                                   LSN     UU(oplsn)) //oplsn is the lsn of the abort
{
    int r = 0;
    //Redirect only has meaning during normal operation (NOT during recovery).
    if (!txn->recovered_from_checkpoint) {
        CACHEFILE new_cf = NULL;
        r = toku_cachefile_of_filenum(txn->logger->ct, new_filenum, &new_cf);
        assert(r == 0);
        FT CAST_FROM_VOIDP(new_h, toku_cachefile_get_userdata(new_cf));

        CACHEFILE old_cf = NULL;
        r = toku_cachefile_of_filenum(txn->logger->ct, old_filenum, &old_cf);
        assert(r == 0);
        FT CAST_FROM_VOIDP(old_h, toku_cachefile_get_userdata(old_cf));

        //Redirect back from new to old.
        r = toku_dictionary_redirect_abort(old_h, new_h, txn);
        assert(r==0);
    }
    return r;
}

int
toku_commit_change_fdescriptor(FILENUM    filenum,
                               BYTESTRING UU(old_descriptor),
                               TOKUTXN    txn,
                               LSN        UU(oplsn))
{
    return do_nothing_with_filenum(txn, filenum);
}

int
toku_rollback_change_fdescriptor(FILENUM    filenum,
                               BYTESTRING old_descriptor,
                               TOKUTXN    txn,
                               LSN        UU(oplsn))
{
    CACHEFILE cf;
    int r;
    r = toku_cachefile_of_filenum(txn->logger->ct, filenum, &cf);
    if (r == ENOENT) { //Missing file on recovered transaction is not an error
        assert(txn->recovered_from_checkpoint);
        r = 0;
        goto done;
    }
    // file must be open, because the txn that created it opened it and
    // noted it, 
    assert(r == 0);

    FT ft;
    ft = NULL;
    r = txn->open_fts.find_zero<FILENUM, find_ft_from_filenum>(filenum, &ft, NULL);
    assert(r == 0);

    DESCRIPTOR_S d;
    toku_fill_dbt(&d.dbt,  old_descriptor.data,  old_descriptor.len);
    toku_ft_update_descriptor(ft, &d);
done:
    return r;
}



