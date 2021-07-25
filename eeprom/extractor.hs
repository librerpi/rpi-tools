{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE NamedFieldPuns #-}

module Main where

import Data.Binary
--import Data.Binary.Get
import qualified Data.ByteString as BS
--import GHC.Generics
--import Control.Applicative
import System.Environment
import Formatting ((%), sformat, hex, shown, stext)
import qualified Data.Text as T
--import qualified Data.Text.Encoding as T
import qualified Data.ByteString.Base16 as Base16

import SpiFs

decodeFirmware :: FilePath -> IO FirmwareFile
decodeFirmware = decodeFile

main :: IO ()
main = do
  args <- getArgs
  let
    prettyPrintEntry :: Entry -> IO ()
    prettyPrintEntry Entry{entryMagic, entryBody} = do
      let
        formatter = "Entry magic=" % hex % " initial_body=" % shown
      putStrLn $ T.unpack $ sformat formatter entryMagic (BS.take 32 entryBody)
    prettyPrintEntry FileEntry{entryFileMagic, entryFileName, entryFileBody, entryFileHashes} = do
      let
        formatter1 = "File Entry magic=" % hex % " name=" % stext % " body=" % shown
        formatter2 = "File Entry magic=" % hex % " name=" % stext % " body=hidden"
        formatter4 = "File Entry magic=" % hex % " name=" % stext % " hash1=" % shown % " hash2=" % shown
        go2 = putStrLn $ T.unpack $ sformat formatter1 entryFileMagic entryFileName (BS.take 100 entryFileBody)
        go3 = putStrLn $ T.unpack $ sformat formatter2 entryFileMagic entryFileName
        go4 hash1 hash2 = putStrLn $ T.unpack $ sformat formatter4 entryFileMagic entryFileName hash1 hash2
      case (entryFileName, entryFileHashes) of
        ("bootconf.txt", _) -> go2
        (_, Just (hash1, hash2)) -> go4 (Base16.encode hash1) (Base16.encode hash2)
        _ -> go3
    prettyPrintEntry EntryPadding = putStrLn $ "Padding"
    go :: [String] -> IO ()
    go [file] = do
      parsed <- decodeFirmware file
      mapM_ prettyPrintEntry (firmwareEntries parsed)
      mapM_ extractFiles (firmwareEntries parsed)
      print $ generateLayoutFile parsed
  go args

extractFiles :: Entry -> IO ()
extractFiles Entry{entryMagic, entryBody}
  | entryMagic == 0x55aaf00f = do
    BS.writeFile "bootcode.bin" entryBody
  | otherwise = pure ()
extractFiles FileEntry{entryFileName, entryFileBody, entryFileBodyCompressed} = do
  BS.writeFile (T.unpack entryFileName) entryFileBody
  let
    maybeWriteFile Nothing = pure ()
    maybeWriteFile (Just body) = BS.writeFile ((T.unpack entryFileName) <> ".lzjb") body
  maybeWriteFile entryFileBodyCompressed
extractFiles EntryPadding = pure ()
